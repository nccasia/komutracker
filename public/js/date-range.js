import { t } from './i18n.js';

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
const UTC_OFFSET_MILLISECONDS = 7 * 60 * 60 * 1000;
const CUSTOM_RANGE_WARNING_DAYS = 14;
const RETAINED_CALENDAR_DAYS = 90;

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

function createTranslatedError(key, values) {
    const error = new Error(t(key, values));
    error.translationKey = key;
    error.translationValues = values;
    return error;
}

export function isSupportedRangeMode(mode) {
    return SUPPORTED_RANGE_MODES.has(mode);
}

export function createDateRangeController({ options, customContainer, customStart, customEnd, customWarning }) {
    const today = localDay();
    const earliestRetainedDay = shiftDay(today, -(RETAINED_CALENDAR_DAYS - 1));
    let mode = RANGE_MODES.TODAY;

    customStart.value = shiftDay(today, -1);
    customEnd.value = today;
    customStart.min = earliestRetainedDay;
    customStart.max = today;
    customEnd.min = earliestRetainedDay;
    customEnd.max = today;

    function selectedCustomDays() {
        if (!customStart.value || !customEnd.value || customEnd.value < customStart.value) return 0;
        return Math.round(
            (dayRange(customEnd.value).start.getTime() - dayRange(customStart.value).start.getTime())
            / MILLISECONDS_PER_DAY,
        ) + 1;
    }

    function updateWarning() {
        const days = selectedCustomDays();
        const shouldWarn = mode === RANGE_MODES.CUSTOM && days > CUSTOM_RANGE_WARNING_DAYS;
        customWarning.hidden = !shouldWarn;
        customWarning.textContent = shouldWarn
            ? t('range.warning', { days })
            : '';
    }

    function select(nextMode) {
        if (!isSupportedRangeMode(nextMode)) return;

        mode = nextMode;
        options.forEach((option) => {
            option.classList.toggle('active', option.dataset.range === mode);
            option.setAttribute('aria-pressed', String(option.dataset.range === mode));
        });
        customContainer.hidden = mode !== RANGE_MODES.CUSTOM;
        updateWarning();
    }

    function restore({ timespan, start, end }) {
        select(timespan);
        if (mode !== RANGE_MODES.CUSTOM) return;

        if (start) customStart.value = start;
        if (end) customEnd.value = end;
        updateWarning();
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
            throw createTranslatedError('range.invalid');
        }
        if (customStart.value < earliestRetainedDay || customEnd.value > today) {
            throw createTranslatedError('range.outOfBounds', { days: RETAINED_CALENDAR_DAYS });
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
    customStart.addEventListener('input', updateWarning);
    customEnd.addEventListener('input', updateWarning);
    select(mode);

    return { restore, getSelectedRange, getQueryState, refreshLanguage: updateWarning };
}
