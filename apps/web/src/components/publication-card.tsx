"use client"

import { useCallback, useState } from "react"
import { Download, Copy, Loader2 } from "lucide-react"
import { ApiError, Publication, generatePublication, getPublication, isAbortError } from "@/lib/api"
import { useI18n } from "@/lib/i18n"
import { SerialPollingContext, useSerialPolling } from "@/lib/use-serial-polling"
import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"

export function PublicationCard({ taskId }: { taskId: string }) {
  const { language } = useI18n()
  const zh = language === "zh"
  const [data, setData] = useState<Publication | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState("")
  const [copied, setCopied] = useState("")
  const poll = useCallback(async ({ signal, isCurrent }: SerialPollingContext) => {
    if (data || busy) return
    try {
      const result = await getPublication(taskId, signal)
      if (isCurrent()) { setData(result.publication); setError("") }
    } catch (error) {
      if (isCurrent() && !isAbortError(error) && !(error instanceof ApiError && error.status === 409)) {
        setError(error instanceof Error ? error.message : String(error))
      }
    }
  }, [taskId, data, busy])
  const invalidatePolling = useSerialPolling(poll, 5000)

  async function generate() {
    invalidatePolling()
    setBusy(true); setError("")
    try { setData((await generatePublication(taskId)).publication) }
    catch (error) { setError(error instanceof Error ? error.message : String(error)) }
    finally { setBusy(false) }
  }
  async function copy(text: string, field: string) {
    try { await navigator.clipboard.writeText(text); setCopied(field) }
    catch { setError(zh ? "复制失败，请手动选择文字复制。" : "Copy failed. Please select and copy the text.") }
  }

  return (
    <Card>
      <CardHeader><CardTitle>{zh ? "中文发布资料" : "Chinese publication copy"}</CardTitle></CardHeader>
      <CardContent className="space-y-4">
        {data ? <>
          <div className="space-y-2">
            <div className="flex items-center justify-between gap-2">
              <span className="text-sm font-medium">{zh ? "中文标题" : "Chinese title"}</span>
              <Button variant="outline" size="sm" onClick={() => copy(data.title_zh, "title")}>
                <Copy className="size-4" />{copied === "title" ? (zh ? "已复制" : "Copied") : (zh ? "复制标题" : "Copy title")}
              </Button>
            </div>
            <p className="select-text break-words rounded-md bg-muted p-3 text-sm">{data.title_zh}</p>
          </div>
          <div className="space-y-2">
            <div className="flex items-center justify-between gap-2">
              <span className="text-sm font-medium">{zh ? "中文简介" : "Chinese description"}</span>
              {data.description_zh ? <Button variant="outline" size="sm" onClick={() => copy(data.description_zh, "description")}>
                <Copy className="size-4" />{copied === "description" ? (zh ? "已复制" : "Copied") : (zh ? "复制简介" : "Copy description")}
              </Button> : null}
            </div>
            <p className="max-h-96 select-text overflow-y-auto whitespace-pre-wrap break-words rounded-md bg-muted p-3 text-sm">
              {data.description_zh || (zh ? "原链接无简介" : "The source has no description")}
            </p>
          </div>
          <details className="text-sm text-muted-foreground">
            <summary className="cursor-pointer">{zh ? "查看原文及来源" : "Original text and attribution"}</summary>
            <p className="mt-2 whitespace-pre-wrap break-words">{data.source_title}{"\n\n"}{data.source_description}{"\n\n"}{data.uploader}{"\n"}{data.source_url}</p>
          </details>
          <Button nativeButton={false} render={<a href={`/api/tasks/${encodeURIComponent(taskId)}/artifact/publication`} />}>
            <Download className="size-4" />{zh ? "下载发布资料 TXT" : "Download publication TXT"}
          </Button>
        </> : <>
          <p className="text-sm text-muted-foreground">{zh ? "忠实翻译原视频标题和完整简介，保留链接与署名。可单独补齐，无需重新处理视频。" : "Faithful Chinese title and full description with links and attribution. Generate independently of video processing."}</p>
          <Button onClick={generate} disabled={busy}>
            {busy ? <Loader2 className="size-4 animate-spin" /> : null}
            {busy ? (zh ? "正在翻译发布资料…" : "Translating…") : (zh ? "生成 / 补齐中文发布资料" : "Generate Chinese publication copy")}
          </Button>
        </>}
        {error ? <p role="alert" className="break-words text-sm text-red-600">{error}</p> : null}
      </CardContent>
    </Card>
  )
}
