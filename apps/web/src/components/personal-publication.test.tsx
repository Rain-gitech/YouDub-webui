import { cleanup, render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, expect, it, vi } from 'vitest'
import type { Bindings } from '@/plugin/sdk'
import { PersonalPublication } from './personal-publication'

const mocks = vi.hoisted(() => ({ copy: vi.fn() }))
vi.mock('@/lib/i18n', () => ({ useI18n: () => ({ language: 'zh' }) }))
vi.mock('@/lib/clipboard', () => ({ copyText: mocks.copy }))
afterEach(() => { cleanup(); vi.clearAllMocks() })

it('copies the full description and downloads through exactly one authenticated file endpoint', async () => {
  const data = { publication: { title_zh: '中文标题', description_zh: '完整简介\nhttps://example.com', source_title: 'Title', source_description: 'Source', source_url: 'https://example.com', uploader: 'Author' } }
  const useQuery = (() => ({ data, refresh: vi.fn() })) as unknown as Bindings<'useQuery'>['useQuery']
  const request = vi.fn() as Bindings<'request'>['request']
  render(<PersonalPublication taskId="example" useQuery={useQuery} request={request} />)
  await userEvent.setup().click(screen.getByRole('button', { name: '复制简介' }))
  expect(mocks.copy).toHaveBeenCalledWith(data.publication.description_zh)
  expect(screen.getByRole('link', { name: '下载发布资料 TXT' })).toHaveAttribute('href', '/api/youdub/file?path=%2Fapi%2Fv2%2Ftasks%2Fexample%2Fpublication.txt')
  expect(screen.getByRole('link', { name: '旧版历史任务与成片' })).toHaveAttribute('href', 'http://127.0.0.1:3000/')
})

it('keeps an absent description empty and does not offer copying invented text', () => {
  const useQuery = (() => ({ data: { publication: { title_zh: '标题', description_zh: '', source_title: 'Title', source_description: '', source_url: '', uploader: '' } }, refresh: vi.fn() })) as unknown as Bindings<'useQuery'>['useQuery']
  render(<PersonalPublication taskId="example" useQuery={useQuery} request={vi.fn()} />)
  expect(screen.getByText('原链接无简介')).toBeVisible()
  expect(screen.queryByRole('button', { name: '复制简介' })).not.toBeInTheDocument()
})
