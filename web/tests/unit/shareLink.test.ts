import { describe, expect, it, vi } from 'vitest';

import {
  clearShareFragment,
  readShareFragment,
  shareUrl,
} from '../../src/game/shareLink.ts';

describe('readShareFragment', () => {
  it('reads the state a fragment carries', () => {
    expect(readShareFragment('#s=AQEAAAA')).toBe('AQEAAAA');
    expect(readShareFragment('s=AQEAAAA')).toBe('AQEAAAA');

    // Beside something else, which is what a query-shaped fragment allows.
    expect(readShareFragment('#other=1&s=AQEAAAA')).toBe('AQEAAAA');
  });

  it('has nothing to read without one', () => {
    expect(readShareFragment('')).toBeNull();
    expect(readShareFragment('#')).toBeNull();
    expect(readShareFragment('#other=1')).toBeNull();
    expect(readShareFragment('#s=')).toBeNull();
  });
});

describe('clearShareFragment', () => {
  it('takes the fragment off without adding a history entry', () => {
    const history = { replaceState: vi.fn() };

    clearShareFragment(
      { href: 'https://example.test/cube/?a=1#s=AQ', hash: '#s=AQ' },
      history,
    );

    expect(history.replaceState).toHaveBeenCalledWith(null, '', '/cube/?a=1');
  });

  it('leaves an address that has no fragment alone', () => {
    const history = { replaceState: vi.fn() };

    clearShareFragment({ href: 'https://example.test/cube/', hash: '' }, history);

    expect(history.replaceState).not.toHaveBeenCalled();
  });
});

describe('shareUrl', () => {
  it('puts the state on a copy of the address', () => {
    expect(shareUrl('https://example.test/cube/', 'AQEAAAA')).toBe(
      'https://example.test/cube/#s=AQEAAAA',
    );
  });

  it('replaces a fragment the address already had', () => {
    // The sender's own address never holds one, so this is only ever the
    // page that opened a link and was cleaned up afterwards -- but the URL is
    // built rather than appended to, so it holds either way.
    expect(shareUrl('https://example.test/cube/#s=OLD', 'NEW')).toBe(
      'https://example.test/cube/#s=NEW',
    );
  });
});
