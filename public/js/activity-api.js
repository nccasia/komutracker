export async function fetchActivityEvents(username, { start, end }) {
    const params = new URLSearchParams({
        start: start.toISOString(),
        end: end.toISOString(),
    });
    const url = `/api/0/users/${encodeURIComponent(username)}/events?${params}`;
    const response = await fetch(url);

    if (response.status === 404) {
        throw new Error('User not found');
    }
    if (!response.ok) {
        const body = await response.json().catch(() => null);
        throw new Error(body?.error || 'Unable to load this report');
    }

    return response.json();
}
