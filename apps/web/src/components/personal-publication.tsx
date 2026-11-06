import { useState } from 'react'
import type { Bindings } from '@/plugin/sdk'
import { useI18n } from '@/lib/i18n'
import { copyText } from '@/lib/clipboard'
import { assetUrl } from '@/lib/asset-url'
import { Button } from '@/components/ui/button'
import { Surface } from '@/components/ui/surface'
import { InlineAlert } from '@/components/inline-alert'

type Publication = { title_zh: string; description_zh: string; source_title: string; source_description: string; source_url: string; uploader: string }
const needsPublication = (value: { publication: Publication | null }) => !value.publication

export function PersonalPublication({ taskId, request, useQuery }: { taskId: string } & Bindings<'request' | 'useQuery'>) {
  const { language } = useI18n(), zh = language === 'zh'
  const path = `/api/v2/tasks/${encodeURIComponent(taskId)}/publication`
  const { data, error, refresh } = useQuery<{ publication: Publication | null }>(path, 5000, needsPublication)
  const [busy, setBusy] = useState(false), [failure, setFailure] = useState(''), [copied, setCopied] = useState('')
  const publication = data?.publication
  async function generate() {
    setBusy(true); setFailure('')
    try { await request(path, { method: 'POST' }); refresh() }
    catch (error) { setFailure(error instanceof Error ? error.message : String(error)) }
    finally { setBusy(false) }
  }
  async function copy(value: string, field: string) {
    try { await copyText(value); setCopied(field) }
    catch { setFailure(zh ? '请手动选择并复制文字。' : 'Please select and copy the text.') }
  }
  return <Surface as="section" className="space-y-4 p-4">
    <div className="flex flex-wrap items-center justify-between gap-2">
      <h2 className="font-semibold">{zh ? '中文发布资料' : 'Chinese publication copy'}</h2>
      <a className="text-sm underline" href="http://127.0.0.1:3000/" target="_blank" rel="noreferrer">{zh ? '旧版历史任务与成片' : 'Classic task history'}</a>
    </div>
    {publication ? <>
      <div className="flex items-center justify-between gap-2"><span>{zh ? '中文标题' : 'Chinese title'}</span><Button variant="outline" size="sm" onClick={() => copy(publication.title_zh, 'title')}>{copied === 'title' ? (zh ? '已复制' : 'Copied') : (zh ? '复制标题' : 'Copy title')}</Button></div>
      <p className="select-text break-words">{publication.title_zh}</p>
      <div className="flex items-center justify-between gap-2"><span>{zh ? '中文简介' : 'Chinese description'}</span>{publication.description_zh && <Button variant="outline" size="sm" onClick={() => copy(publication.description_zh, 'description')}>{copied === 'description' ? (zh ? '已复制' : 'Copied') : (zh ? '复制简介' : 'Copy description')}</Button>}</div>
      <p className="max-h-96 select-text overflow-y-auto whitespace-pre-wrap break-words">{publication.description_zh || (zh ? '原链接无简介' : 'No description in the source')}</p>
      <details><summary>{zh ? '查看原文与署名' : 'Original text and attribution'}</summary><p className="whitespace-pre-wrap break-words">{publication.source_title}{'\n\n'}{publication.source_description}{'\n\n'}{publication.uploader}{'\n'}{publication.source_url}</p></details>
      <a className="text-sm underline" href={assetUrl(path + '.txt')}>{zh ? '下载发布资料 TXT' : 'Download publication TXT'}</a>
    </> : <><p className="text-sm text-muted-foreground">{zh ? '任务完成后自动生成忠实翻译的中文标题和完整简介，保留链接与署名。也可单独补齐。' : 'Faithful Chinese title and full description are generated after completion, preserving links and attribution.'}</p><Button onClick={generate} disabled={busy}>{busy ? (zh ? '正在翻译…' : 'Translating…') : (zh ? '生成 / 补齐发布资料' : 'Generate publication copy')}</Button></>}
    {(failure || error) && <InlineAlert>{failure || error}</InlineAlert>}
  </Surface>
}
