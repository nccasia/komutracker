export const ACTIVE_STATUS = 'not-afk';
export const AFK_STATUS = 'afk';

export function isActiveEvent(event) {
    return event.status === ACTIVE_STATUS;
}

export function clipEventToRange(event, range) {
    const from = Math.max(new Date(event.startAt).getTime(), range.start.getTime());
    const to = Math.min(new Date(event.endAt).getTime(), range.end.getTime());
    return { from, to, seconds: Math.max(0, (to - from) / 1000) };
}
