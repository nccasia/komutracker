import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

describe('legacy activity URL fallback', () => {
    it('redirects an old activity route to the current username lookup', async () => {
        const { legacyActivityRedirectUrl } = await import('../../public/js/legacy-url.js');
        assert.equal(
            legacyActivityRedirectUrl(
                '#/activity/workstation-01/view',
                'https://tracker.komu.vn/#/activity/workstation-01/view',
            ),
            'https://tracker.komu.vn/?username=workstation-01',
        );
    });

    it('decodes and safely re-encodes the hostname', async () => {
        const { legacyActivityRedirectUrl } = await import('../../public/js/legacy-url.js');
        assert.equal(
            legacyActivityRedirectUrl(
                '#/activity/Nguy%E1%BB%85n%20V%C4%83n%20A/view/',
                'https://tracker.komu.vn/#/activity/Nguy%E1%BB%85n%20V%C4%83n%20A/view/',
            ),
            'https://tracker.komu.vn/?username=Nguy%E1%BB%85n+V%C4%83n+A',
        );
    });

    it('ignores unrelated and malformed fragments', async () => {
        const { legacyActivityRedirectUrl } = await import('../../public/js/legacy-url.js');
        assert.equal(legacyActivityRedirectUrl('#/settings', 'https://tracker.komu.vn/'), null);
        assert.equal(legacyActivityRedirectUrl('#/activity/%ZZ/view', 'https://tracker.komu.vn/'), null);
        assert.equal(legacyActivityRedirectUrl('#/activity//view', 'https://tracker.komu.vn/'), null);
    });
});
