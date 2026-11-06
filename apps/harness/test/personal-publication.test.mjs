import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { generatePublication, publicationText, registerPublicationRoutes } from '../src/backend/publication.mjs';

test('publication preserves complete metadata, deduplicates concurrent generation and caches it', async () => {
  const root = await mkdtemp(join(tmpdir(), 'youdub-publication-'));
  try {
    let calls = 0;
    const source = { title: 'Title', description: 'Full original paragraph. '.repeat(500) + '\nhttps://example.com', sourceUrl: 'https://youtube.com/watch?v=id', uploader: 'Author' };
    const translate = async input => { calls++; assert.equal(input.description, source.description); return { title_zh: '中文标题', description_zh: '中文简介\nhttps://example.com' }; };
    const [a, b] = await Promise.all([generatePublication(root, source, translate), generatePublication(root, source, translate)]);
    assert.equal(a.title_zh, '中文标题'); assert.deepEqual(a, b); assert.equal(calls, 1);
    await generatePublication(root, source, translate); assert.equal(calls, 1);
    assert.match(publicationText(a), /中文简介/);
    const empty = await generatePublication(root, { ...source, description: '' }, async () => ({ title_zh: '标题', description_zh: 'invented' }));
    assert.equal(empty.description_zh, '');
  } finally { await rm(root, { recursive: true, force: true }); }
});

test('publication rejects blank titles and omitted source URLs', async () => {
  const root = await mkdtemp(join(tmpdir(), 'youdub-publication-'));
  try {
    const source = { title: 'Title', description: 'https://example.com', sourceUrl: '', uploader: '' };
    await assert.rejects(generatePublication(root, source, async () => ({ title_zh: '', description_zh: '简介' })));
    await assert.rejects(generatePublication(root, source, async () => ({ title_zh: '标题', description_zh: '简介' })));
  } finally { await rm(root, { recursive: true, force: true }); }
});

test('Host disposal aborts an in-flight model stream and releases its task read lock', async () => {
  const root = await mkdtemp(join(tmpdir(), 'youdub-publication-'));
  try {
    const routes = new Map(), disposers = []; let locks = 0, entered;
    const started = new Promise(resolve => { entered = resolve; });
    const ctx = {
      effect: setup => disposers.push(setup()), logger: { warn() {} },
      files: { taskRoot: () => root, readLock: () => { locks++; return () => { locks--; }; } },
      tasks: { record: async () => ({ sourceName: 'Title', artifacts: {}, plan: { bindings: { translation: { providerId: 'llm.personal', model: 'test' } } } }) },
      llm: { prepareCall: async (_selection, signal) => ({ config: {}, async *stream(options) {
        assert.equal(options.signal, signal); entered();
        await new Promise((_resolve, reject) => signal.addEventListener('abort', () => reject(signal.reason), { once: true }));
      } }) },
    };
    registerPublicationRoutes(ctx, { register(method, path, handler) { routes.set(method + path, handler); return () => routes.delete(method + path); }, json() {} });
    const request = routes.get('POST/api/v2/tasks/:id/publication')({ params: { id: 'example' } });
    const rejected = assert.rejects(request, /stopping/);
    await started; assert.equal(locks, 1);
    await disposers.at(-1)(); await rejected;
    assert.equal(locks, 0);
  } finally { await rm(root, { recursive: true, force: true }); }
});
