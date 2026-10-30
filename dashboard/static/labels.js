window.SENSOR_LABELS = window.POPX_DESIGN.sensors;
window.ACTION_LABELS = window.POPX_DESIGN.actions;
// Input 32 was a zero-valued compatibility slot before the 57-input model.
// SENSOR_COUNT (57) is a sentinel and must never appear as an input label.
window.sensorLabelsForSize = (size, model = 'chemical_density_v2') => {
    const labels = {...window.SENSOR_LABELS};
    if (model !== 'chemical_density_v2' || size < 57)
        for (const [offset, direction] of ['N', 'W', 'E', 'S'].entries())
            labels[24 + offset] = `Grad. FOOD ${direction} (precedente)`;
    if (size < 57) labels[32] = 'Ingresso riservato (sempre zero)';
    for (let index = 57; index < size; index++)
        labels[index] = `Ingresso ${index} · non implementato`;
    return labels;
};
window.sensorLabelsForSnapshot = snapshot =>
    window.sensorLabelsForSize(snapshot.brain.sizeS, snapshot.sensor_model || 'legacy_food_gradients');
