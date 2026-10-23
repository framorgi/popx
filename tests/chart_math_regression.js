'use strict';
const assert=require('assert');
const math=require('../dashboard/static/chart_math.js').PopXChartMath;
assert(math.isDerivable('alive'));
assert(math.isDerivable('counts.natural_births'));
assert(math.isDerivable('reproduction.births'));
assert(math.isDerivable('totals.other_action_cost'));
assert(math.isDerivable('world_resources.0'));
assert(!math.isDerivable('learning.reward'));
assert(!math.isDerivable('reproduction.capacity'));
assert(!math.isDerivable('averages.body_temperature'));
const samples=[{tick:0,alive:8,counts:{natural_births:0}},
               {tick:200,alive:12,counts:{natural_births:6}},
               {tick:600,alive:4,counts:{natural_births:10}}];
assert.deepStrictEqual(math.points(samples,'alive',true),
                       [{x:0,y:null},{x:200,y:.02},{x:600,y:-.02}]);
assert.deepStrictEqual(math.points(samples,'counts.natural_births',true),
                       [{x:0,y:null},{x:200,y:.03},{x:600,y:.01}]);
assert.deepStrictEqual(math.points(samples,'unknown',true),
                       [{x:0,y:null},{x:200,y:null},{x:600,y:null}]);
assert.deepStrictEqual(math.points(samples,'alive'),
                       [{x:0,y:8},{x:200,y:12},{x:600,y:4}]);
const strokes=[],positions=[];
const ctx={save(){},beginPath(){},rect(){},clip(){},restore(){},
           moveTo(x,y){positions.push([x,y]);},lineTo(x,y){positions.push([x,y]);},stroke(){strokes.push(1);}};
math.extinctionPlugin(()=>[100,350,900]).afterDatasetsDraw({
    scales:{x:{min:0,max:600,getPixelForValue:tick=>tick/2}},
    chartArea:{left:0,right:300,top:10,bottom:90},ctx
});
assert.strictEqual(strokes.length,2);
assert.deepStrictEqual(positions,[[50,10],[50,90],[175,10],[175,90]]);
console.log('PASS: stock/cumulative derivatives, irregular intervals, missing values and eligibility');
