import { isSupportedRangeMode } from './date-range.js';

export function readLookupParams(search = window.location.search) {
    const params = new URLSearchParams(search);
    const timespan = params.get('timespan')?.trim();

    return {
        username: params.get('username')?.trim() || '',
        timespan: isSupportedRangeMode(timespan) ? timespan : undefined,
        start: params.get('start')?.trim() || '',
        end: params.get('end')?.trim() || '',
    };
}

export function persistLookupParams({ username, timespan, start, end }) {
    const url = new URL(window.location.href);
    url.searchParams.set('username', username);
    url.searchParams.set('timespan', timespan);

    if (start && end) {
        url.searchParams.set('start', start);
        url.searchParams.set('end', end);
    } else {
        url.searchParams.delete('start');
        url.searchParams.delete('end');
    }

    window.history.replaceState({}, '', url);
}
