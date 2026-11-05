import { cleanup, render, screen, waitFor } from "@testing-library/react"
import userEvent from "@testing-library/user-event"
import { afterEach, expect, it, vi } from "vitest"
import { LanguageProvider } from "@/lib/i18n"
import { PublicationCard } from "@/components/publication-card"

const mocks = vi.hoisted(() => ({ read: vi.fn(), generate: vi.fn() }))
vi.mock("@/lib/api", async (original) => ({
  ...await original<typeof import("@/lib/api")>(),
  getPublication: mocks.read,
  generatePublication: mocks.generate,
}))
afterEach(() => { cleanup(); vi.clearAllMocks(); window.localStorage.clear() })

it("shows full Chinese publication copy and copies description independently", async () => {
  const data = { title_zh: "中文标题", description_zh: "完整简介\nhttps://example.com", source_title: "Original", source_description: "Source", uploader: "Author", source_url: "https://example.com" }
  mocks.read.mockResolvedValue({ publication: data })
  const user = userEvent.setup()
  render(<LanguageProvider><PublicationCard taskId="example" /></LanguageProvider>)
  expect(await screen.findByText("中文标题", { selector: "p" })).toBeVisible()
  await user.click(screen.getByRole("button", { name: "复制简介" }))
  expect(await navigator.clipboard.readText()).toBe(data.description_zh)
  expect(screen.getByRole("button", { name: "下载发布资料 TXT" })).toHaveAttribute("href", "/api/tasks/example/artifact/publication")
})

it("can backfill a task independently and explicitly identifies missing source descriptions", async () => {
  mocks.read.mockResolvedValue({ publication: null })
  mocks.generate.mockResolvedValue({ publication: { title_zh: "标题", description_zh: "", source_title: "Title", source_description: "", uploader: "", source_url: "" } })
  const user = userEvent.setup()
  render(<LanguageProvider><PublicationCard taskId="old" /></LanguageProvider>)
  await user.click(screen.getByRole("button", { name: "生成 / 补齐中文发布资料" }))
  await waitFor(() => expect(mocks.generate).toHaveBeenCalledWith("old"))
  expect(await screen.findByText("原链接无简介")).toBeVisible()
  expect(screen.queryByRole("button", { name: "复制简介" })).not.toBeInTheDocument()
})
