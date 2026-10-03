const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
const UTC_OFFSET_MILLISECONDS = 7 * 60 * 60 * 1000;

export const RANGE_MODES = Object.freeze({
    TODAY: 'today',
    YESTERDAY: 'yesterday',
    SEVEN_DAYS: '7days',
    CUSTOM: 'custom',
});

const SUPPORTED_RANGE_MODES = new Set(Object.values(RANGE_MODES));

function localDay(date = new Date()) {
    return new Date(date.getTime() + UTC_OFFSET_MILLISECONDS).toISOString().slice(0, 10);
}

function shiftDay(day, amount) {
    const date = new Date(`${day}T00:00:00+07:00`);
    date.setUTCDate(date.getUTCDate() + amount);
    return localDay(date);
}

function dayRange(day) {
    const start = new Date(`${day}T00:00:00+07:00`);
    return { start, end: new Date(start.getTime() + MILLISECONDS_PER_DAY) };
}

export function isSupportedRangeMode(mode) {
    return SUPPORTED_RANGE_MODES.has(mode);
}

export function createDateRangeController({ options, customContainer, customStart, customEnd }) {
    const today = localDay();
    let mode = RANGE_MODES.TODAY;

    customStart.value = shiftDay(today, -1);
    customEnd.value = today;

    function select(nextMode) {
        if (!isSupportedRangeMode(nextMode)) return;

        mode = nextMode;
        options.forEach((option) => {
            option.classList.toggle('active', option.dataset.range === mode);
            option.setAttribute('aria-pressed', String(option.dataset.range === mode));
        });
        customContainer.hidden = mode !== RANGE_MODES.CUSTOM;
    }

    function restore({ timespan, start, end }) {
        select(timespan);
        if (mode !== RANGE_MODES.CUSTOM) return;

        if (start) customStart.value = start;
        if (end) customEnd.value = end;
    }

    function getSelectedRange() {
        if (mode === RANGE_MODES.TODAY) {
            return { ...dayRange(today), label: today };
        }

        if (mode === RANGE_MODES.YESTERDAY) {
            const day = shiftDay(today, -1);
            return { ...dayRange(day), label: day };
        }

        if (mode === RANGE_MODES.SEVEN_DAYS) {
            const startDay = shiftDay(today, -6);
            return {
                start: dayRange(startDay).start,
                end: dayRange(today).end,
                label: `${startDay} – ${today}`,
            };
        }

        if (!customStart.value || !customEnd.value || customEnd.value < customStart.value) {
            throw new Error('Choose a valid custom start and end date.');
        }

        return {
            start: dayRange(customStart.value).start,
            end: dayRange(customEnd.value).end,
            label: `${customStart.value} – ${customEnd.value}`,
        };
    }

    function getQueryState() {
        if (mode !== RANGE_MODES.CUSTOM) return { timespan: mode };
        return { timespan: mode, start: customStart.value, end: customEnd.value };
    }

    options.forEach((option) => {
        option.addEventListener('click', () => select(option.dataset.range));
    });
    select(mode);

    return { restore, getSelectedRange, getQueryState };
}
