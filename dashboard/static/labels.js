window.SENSOR_LABELS = {
    '0': 'LOC_X',
    '1': 'LOC_Y · coordinata dall’alto',
    '2': 'BOUNDARY_DIST_X',
    '3': 'BOUNDARY_DIST',
    '4': 'BOUNDARY_DIST_Y',
    '5': 'GENETIC_SIM_FWD · somiglianza genetica del Pop davanti',
    '6': 'LAST_MOVE_DIR_X',
    '7': 'LAST_MOVE_DIR_Y',
    '8': 'POPULATION_DENSITY_N',
    '9': 'POPULATION_DENSITY_W',
    '10': 'POPULATION_DENSITY_E',
    '11': 'POPULATION_DENSITY_S',
    '12': 'TEMP_AVG_N',
    '13': 'TEMP_AVG_W',
    '14': 'TEMP_AVG_E',
    '15': 'TEMP_AVG_S',
    '16': 'TEMP_DRV_N',
    '17': 'TEMP_DRV_W',
    '18': 'TEMP_DRV_E',
    '19': 'TEMP_DRV_S',
    '20': 'SENSE_SIGNAL_FOOD · intensità nella cella corrente',
    '21': 'SENSE_SIGNAL_DANGER · intensità nella cella corrente',
    '22': 'SENSE_SIGNAL_MATE · intensità nella cella corrente',
    '23': 'SENSE_SIGNAL_HOME · intensità nella cella corrente',
    '24': 'SENSE_SIGNAL_DENSITY_FOOD · densità FOOD',
    '25': 'SENSE_SIGNAL_DENSITY_DANGER · densità DANGER',
    '26': 'SENSE_SIGNAL_DENSITY_MATE · densità MATE',
    '27': 'SENSE_SIGNAL_DENSITY_HOME · densità HOME',
    '28': 'OSC1',
    '29': 'AGE',
    '30': 'TEMP',
    '31': 'RANDOM',
    '32': 'LAST_MOVE_FAILED · ultimo tentativo di movimento fallito',
    '33': 'GLUCOSE_DENSITY_N',
    '34': 'GLUCOSE_DENSITY_W',
    '35': 'GLUCOSE_DENSITY_E',
    '36': 'GLUCOSE_DENSITY_S',
    '37': 'SENSE_DANGER_DRV_N',
    '38': 'SENSE_DANGER_DRV_W',
    '39': 'SENSE_DANGER_DRV_E',
    '40': 'SENSE_DANGER_DRV_S',
    '41': 'SENSE_MATE_DRV_N',
    '42': 'SENSE_MATE_DRV_W',
    '43': 'SENSE_MATE_DRV_E',
    '44': 'SENSE_MATE_DRV_S',
    '45': 'SENSE_HOME_DRV_N',
    '46': 'SENSE_HOME_DRV_W',
    '47': 'SENSE_HOME_DRV_E',
    '48': 'SENSE_HOME_DRV_S',
    '49': 'MOVE_OPEN_N',
    '50': 'MOVE_OPEN_NE',
    '51': 'MOVE_OPEN_E',
    '52': 'MOVE_OPEN_SE',
    '53': 'MOVE_OPEN_S',
    '54': 'MOVE_OPEN_SW',
    '55': 'MOVE_OPEN_W',
    '56': 'MOVE_OPEN_NW'
};
window.ACTION_LABELS = {
    '0': 'MOVE_FORWARD',
    '1': 'MOVE_LEFT',
    '2': 'MOVE_RIGHT',
    '3': 'MOVE_RANDOM',
    '4': 'MOVE_EAST',
    '5': 'MOVE_WEST',
    '6': 'MOVE_NORTH',
    '7': 'MOVE_SOUTH',
    '8': 'EMIT_SIGNAL_FOOD',
    '9': 'EMIT_SIGNAL_DANGER',
    '10': 'EMIT_SIGNAL_MATE',
    '11': 'EMIT_SIGNAL_HOME',
    '12': 'GET_GLUCOSE',
    '13': 'LEAVE_GLUCOSE',
    '14': 'GET_H2O',
    '15': 'LEAVE_H2O',
    '16': 'GET_CALCIUM',
    '17': 'LEAVE_CALCIUM',
    '18': 'BURN_CALORIES',
    '19': 'SET_OSCILLATOR_PERIOD',
    '20': 'SET_RESPONSIVENESS · aumenta passo e raggio sensoriale',
    '21': 'DO_NOTHING',
    '22': 'KILL_FORWARD'
};
// Input 32 was a zero-valued compatibility slot before the 57-input model.
// SENSOR_COUNT (57) is a sentinel and must never appear as an input label.
window.sensorLabelsForSize = (size, model = 'chemical_density_v2') => {
    const labels = {...window.SENSOR_LABELS};
    if (model !== 'chemical_density_v2' || size < 57)
        for (const [offset, direction] of ['N', 'W', 'E', 'S'].entries())
            labels[24 + offset] = `SENSE_SIGNAL_DRV_${direction} · gradiente FOOD (legacy)`;
    if (size < 57) labels[32] = 'LEGACY_RESERVED_INPUT_32 · sempre zero';
    for (let index = 57; index < size; index++)
        labels[index] = `INPUT_${index} · non implementato`;
    return labels;
};
window.sensorLabelsForSnapshot = snapshot =>
    window.sensorLabelsForSize(snapshot.brain.sizeS, snapshot.sensor_model || 'legacy_food_gradients');
