import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { intervalGapMs } from '../activity';

const at = (seconds: number) => new Date(seconds * 1000);

describe('intervalGapMs', () => {
    it('treats a growing AFK heartbeat as overlapping after the merge window', () => {
        const storedStart = at(0);
        const storedEnd = at(370);
        const heartbeatStart = at(0);
        const heartbeatEnd = at(380);

        assert.equal(intervalGapMs(storedStart, storedEnd, heartbeatStart, heartbeatEnd), 0);
    });

    it('returns the distance between non-overlapping heartbeat intervals', () => {
        assert.equal(intervalGapMs(at(0), at(10), at(20), at(30)), 10_000);
        assert.equal(intervalGapMs(at(20), at(30), at(0), at(10)), 10_000);
    });

    it('returns zero for partial and contained overlaps', () => {
        assert.equal(intervalGapMs(at(0), at(20), at(10), at(30)), 0);
        assert.equal(intervalGapMs(at(0), at(30), at(10), at(20)), 0);
    });
});
