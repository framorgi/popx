"""UDP fragmentation, duplicates, reordering, malformed input and durable replay."""
import json
from pathlib import Path
import tempfile
import unittest
from dashboard.server import Store, Receiver, compact

class Transport:
    def __init__(self):self.sent=[]
    def sendto(self,packet,peer):self.sent.append((json.loads(packet),peer))

class ProtocolTests(unittest.TestCase):
    def test_fragments_and_deduplication(self):
        with tempfile.TemporaryDirectory() as temp:
            store=Store(Path(temp)/'data.sqlite')
            receiver=Receiver(store);transport=Transport();receiver.connection_made(transport)
            msg={'version':1,'process_id':'test','run_id':'run','tick':123,'type':'sample','data':{'tick':123,'long':'x'*6000}}
            body=compact(msg);chunks=[body[i:i+800] for i in range(0,len(body),800)]
            packets=[compact({'v':1,'p':'test','s':4,'i':i,'n':len(chunks),'d':chunk}).encode() for i,chunk in enumerate(chunks)]
            peer=('127.0.0.1',12345)
            for packet in reversed(packets[1:]):receiver.datagram_received(packet,peer)
            self.assertFalse(transport.sent)
            # Retry can arrive in arbitrary order; duplicate parts must not double count.
            for packet in packets+packets:receiver.datagram_received(packet,peer)
            self.assertEqual(store.db.execute('SELECT count(*) FROM samples').fetchone()[0],1)
            self.assertEqual(store.db.execute('SELECT count(*) FROM messages').fetchone()[0],1)
            self.assertTrue(transport.sent)
            self.assertEqual(receiver.partial_bytes,0)
            receiver.datagram_received(b'{bad',peer)
            receiver.datagram_received(compact({'v':1,'p':'x','s':3,'i':0,'n':9999999,'d':'x'}).encode(),peer)
            self.assertEqual(len(receiver.parts),0)
            store.close()
            reopened=Store(Path(temp)/'data.sqlite')
            self.assertEqual(reopened.db.execute('SELECT tick FROM samples').fetchone()[0],123)
            reopened.close()

    def test_status_ordering(self):
        with tempfile.TemporaryDirectory() as temp:
            store=Store(Path(temp)/'data.sqlite')
            def status(sequence,state):
                store.ingest('p',sequence,{'run_id':'run','type':'status','data':{'state':state,'metadata':{}}})
            status(10,'finished');status(9,'running')
            self.assertEqual(store.db.execute('SELECT state FROM runs').fetchone()[0],'finished')
            store.close()

if __name__=='__main__':unittest.main()
