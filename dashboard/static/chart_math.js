'use strict';
// Time-series transformations shared by overview charts and the metric explorer.
(function (root) {
    const stock = new Set(['alive', 'world_nitrogen', 'calcium_saturated_cells']);
    const at = (sample, path) => path.split('.').reduce((value, key) => value?.[key], sample) ?? null;
    function isDerivable(path) {
        if (stock.has(path) || /^world_resources\.\d+$/.test(path)) return true;
        if (path.startsWith('counts.')) return true;
        if (path.startsWith('reproduction.')) return path !== 'reproduction.capacity';
        return ['active.', 'totals.', 'resource_ledger.', 'energy_ledger.']
            .some(prefix => path.startsWith(prefix));
    }
    function points(samples, path, derivative = false) {
        return samples.map((sample, index) => {
            const value = at(sample, path);
            if (!derivative) return {x: sample.tick, y: value};
            const previous = samples[index - 1];
            const previousValue = previous && at(previous, path);
            const interval = previous ? sample.tick - previous.tick : 0;
            return {x: sample.tick,
                    y: interval > 0 && Number.isFinite(value) && Number.isFinite(previousValue)
                        ? (value - previousValue) / interval : null};
        });
    }
    function extinctionPlugin(getTicks) {
        return {id: 'popxExtinctions', afterDatasetsDraw(chart) {
            const ticks = getTicks();
            if (!ticks?.length) return;
            const scale = chart.scales.x, area = chart.chartArea, ctx = chart.ctx;
            ctx.save();
            ctx.beginPath();
            ctx.rect(area.left, area.top, area.right - area.left, area.bottom - area.top);
            ctx.clip();
            ctx.strokeStyle = '#ff4d57';
            ctx.lineWidth = 1.5;
            for (const tick of ticks) {
                if (tick < scale.min || tick > scale.max) continue;
                const x = scale.getPixelForValue(tick);
                ctx.beginPath();
                ctx.moveTo(x, area.top);
                ctx.lineTo(x, area.bottom);
                ctx.stroke();
            }
            ctx.restore();
        }};
    }
    return root.PopXChartMath = {at, isDerivable, points, extinctionPlugin};
})(typeof window === 'undefined' ? module.exports : window);
