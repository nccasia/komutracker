export function legacyActivityRedirectUrl(hash, baseHref) {
    const match = /^#\/activity\/([^/]+)\/view\/?$/.exec(hash);
    if (!match) return null;

    let hostname;
    try {
        hostname = decodeURIComponent(match[1]).trim();
    } catch {
        return null;
    }

    if (!hostname) return null;

    const url = new URL('/', baseHref);
    url.searchParams.set('username', hostname);
    return url.href;
}

export function redirectLegacyActivityUrl(location = window.location) {
    const redirectUrl = legacyActivityRedirectUrl(location.hash, location.href);
    if (!redirectUrl) return false;

    location.replace(redirectUrl);
    return true;
}
