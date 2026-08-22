import { describe, expect, it, vi } from 'vitest';

import {
  clearShareFragment,
  pageUrl,
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

describe('pageUrl', () => {
  it('is the address itself when it carries nothing', () => {
    expect(pageUrl('https://example.test/cube/')).toBe(
      'https://example.test/cube/',
    );
  });

  it('takes off a fragment the address still had', () => {
    // The lifecycle clears the one a shared link opened with, so this is
    // belt and braces -- but what Share hands over for an untouched cube
    // must not be a link to somebody else's cube.
    expect(pageUrl('https://example.test/cube/#s=OLD')).toBe(
      'https://example.test/cube/',
    );
  });

  it('keeps the path and the query, which are not the state', () => {
    expect(pageUrl('https://example.test/cube/?ref=talk#s=OLD')).toBe(
      'https://example.test/cube/?ref=talk',
    );
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
