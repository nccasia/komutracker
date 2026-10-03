const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
const GMT7_OFFSET_MILLISECONDS = 7 * 60 * 60 * 1000;

export const RETAINED_CALENDAR_DAYS = 90;

export interface RetentionWindow {
    start: Date;
    end: Date;
}

export function retentionWindow(now = new Date()): RetentionWindow {
    const localDate = new Date(now.getTime() + GMT7_OFFSET_MILLISECONDS);
    const todayStart = Date.UTC(
        localDate.getUTCFullYear(),
        localDate.getUTCMonth(),
        localDate.getUTCDate(),
    ) - GMT7_OFFSET_MILLISECONDS;

    return {
        start: new Date(todayStart - (RETAINED_CALENDAR_DAYS - 1) * MILLISECONDS_PER_DAY),
        end: new Date(todayStart + MILLISECONDS_PER_DAY),
    };
}

export function assertRangeWithinRetention(start: Date, end: Date, now = new Date()): void {
    const retained = retentionWindow(now);
    if (start < retained.start || end > retained.end) {
        throw new Error(`Invalid query range: dates must be within the last ${RETAINED_CALENDAR_DAYS} days in GMT+7`);
    }
}
