import * as vscode from 'vscode';
import { PredictionResponse } from './cppBackendClient';

export class LearningCoachViewProvider implements vscode.WebviewViewProvider {
    public static readonly viewType = 'autocode.learningCoachView';
    private view?: vscode.WebviewView;
    private latestResponse: PredictionResponse | null = null;
    private selectedIdx: number = 0;
    private forceFullExplain: boolean = false;

    constructor(
        private readonly onInsertCode: (codeSuffix: string, nextLine: string, trigger: string) => void,
        private readonly onSelectCandidate: (index: number) => void
    ) {}

    public resolveWebviewView(
        webviewView: vscode.WebviewView,
        _context: vscode.WebviewViewResolveContext,
        _token: vscode.CancellationToken
    ): void {
        this.view = webviewView;
        webviewView.webview.options = {
            enableScripts: true
        };

        webviewView.webview.onDidReceiveMessage((msg) => {
            if (msg.type === 'insert' && this.latestResponse) {
                const cand = this.latestResponse.candidates[this.selectedIdx];
                if (cand) {
                    this.onInsertCode(cand.completionSuffix, cand.nextLinePreview, cand.triggerPrefix);
                }
            } else if (msg.type === 'selectCandidate') {
                this.selectedIdx = msg.index;
                this.onSelectCandidate(this.selectedIdx);
                this.render();
            } else if (msg.type === 'forceExplain') {
                this.forceFullExplain = true;
                this.render();
            }
        });

        this.render();
    }

    public updatePrediction(
        resp: PredictionResponse | null,
        selectedIdx: number,
        forceFullExplain: boolean = false
    ): void {
        this.latestResponse = resp;
        this.selectedIdx = selectedIdx;
        this.forceFullExplain = forceFullExplain;
        this.render();
    }

    private escapeHtml(str: string): string {
        return str
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    private render(): void {
        if (!this.view) {
            return;
        }

        const resp = this.latestResponse;
        if (!resp || resp.candidates.length === 0) {
            this.view.webview.html = `<!DOCTYPE html>
<html>
<head>
<style>
  body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); padding: 12px; }
  .empty { opacity: 0.75; line-height: 1.5; font-size: 13px; border: 1px dashed var(--vscode-panel-border); padding: 14px; border-radius: 6px; }
  code { background: var(--vscode-textCodeBlock-background); padding: 2px 5px; border-radius: 3px; }
</style>
</head>
<body>
  <div class="empty">
    <h3>🎓 AutoCode C++ Learning Coach</h3>
    <p>Start typing C++ code or pause on any line for <strong>2–4 seconds</strong>.</p>
    <p>Try typing:</p>
    <ul>
      <li><code>nums.</code> or <code>std::so</code></li>
      <li><code>for (</code> or <code>while (</code></li>
      <li>A comment like <code>// sort in descending order</code> and pause on the next line!</li>
    </ul>
  </div>
</body>
</html>`;
            return;
        }

        const safeIdx = Math.min(this.selectedIdx, resp.candidates.length - 1);
        const cand = resp.candidates[safeIdx];
        const effectiveStage = this.forceFullExplain ? 'STUCK' : resp.stage;

        let badgeClass = 'badge-typing';
        let badgeLabel = '⚡ Active Typing (Ghost Mode)';
        if (effectiveStage === 'PONDERING') {
            badgeClass = 'badge-pondering';
            badgeLabel = `💡 Paused ${(resp.idleMs / 1000).toFixed(1)}s — Hint Unlocked`;
        } else if (effectiveStage === 'STUCK') {
            badgeClass = 'badge-stuck';
            badgeLabel = `🎓 Stuck Coach Active — Full Learning Guide`;
        }

        const tabsHtml = resp.candidates
            .map((c, idx) => {
                const active = idx === safeIdx ? 'active' : '';
                return `<button class="tab-btn ${active}" onclick="selectCand(${idx})">#${idx + 1} ${this.escapeHtml(c.sourceEngine)} (${Math.round(c.confidence * 100)}%)</button>`;
            })
            .join('');

        const deepLearningHtml =
            effectiveStage === 'STUCK'
                ? `
        <div class="card explain-card">
            <div class="card-title">📖 Why This Code Works</div>
            <p>${this.escapeHtml(cand.detailedExplanation)}</p>
            ${
                cand.complexityNote
                    ? `<div class="meta-row"><strong>⏱ Complexity:</strong> ${this.escapeHtml(cand.complexityNote)}</div>`
                    : ''
            }
            ${
                cand.commonPitfall
                    ? `<div class="meta-row pitfall"><strong>⚠️ Common Pitfall:</strong> ${this.escapeHtml(cand.commonPitfall)}</div>`
                    : ''
            }
        </div>`
                : `
        <button class="secondary-btn" onclick="forceExplain()">
            📖 Show Full Explanation & Complexity Now
        </button>`;

        this.view.webview.html = `<!DOCTYPE html>
<html>
<head>
<style>
  body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); padding: 10px; font-size: 13px; line-height: 1.45; }
  .badge { display: inline-block; padding: 4px 8px; border-radius: 4px; font-weight: 600; font-size: 11px; margin-bottom: 8px; }
  .badge-typing { background: #1e4620; color: #a7f3d0; }
  .badge-pondering { background: #5c4004; color: #fde68a; }
  .badge-stuck { background: #1e3a5f; color: #93c5fd; }
  .context-bar { font-size: 11px; opacity: 0.75; margin-bottom: 10px; font-family: var(--vscode-editor-font-family); }
  .tabs { display: flex; gap: 4px; flex-wrap: wrap; margin-bottom: 10px; }
  .tab-btn { background: var(--vscode-button-secondaryBackground); color: var(--vscode-button-secondaryForeground); border: none; padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 11px; }
  .tab-btn.active { background: var(--vscode-button-background); color: var(--vscode-button-foreground); font-weight: bold; }
  .concept-header { font-size: 15px; font-weight: 700; margin: 6px 0 8px 0; }
  .code-box { background: var(--vscode-textCodeBlock-background); padding: 10px; border-radius: 6px; font-family: var(--vscode-editor-font-family); font-size: 12.5px; white-space: pre-wrap; margin-bottom: 8px; border-left: 3px solid var(--vscode-button-background); }
  .primary-btn { width: 100%; background: var(--vscode-button-background); color: var(--vscode-button-foreground); border: none; padding: 8px; border-radius: 4px; cursor: pointer; font-weight: 600; margin-bottom: 10px; }
  .primary-btn:hover { background: var(--vscode-button-hoverBackground); }
  .secondary-btn { width: 100%; background: var(--vscode-button-secondaryBackground); color: var(--vscode-button-secondaryForeground); border: none; padding: 7px; border-radius: 4px; cursor: pointer; margin-top: 6px; }
  .card { background: var(--vscode-editorWidget-background); border: 1px solid var(--vscode-panel-border); border-radius: 6px; padding: 10px; margin-bottom: 10px; }
  .card-title { font-weight: 700; margin-bottom: 4px; color: var(--vscode-textLink-foreground); }
  .meta-row { margin-top: 8px; padding-top: 6px; border-top: 1px solid var(--vscode-panel-border); font-size: 12px; }
  .pitfall { color: #fca5a5; }
</style>
</head>
<body>
  <div class="badge ${badgeClass}">${badgeLabel}</div>
  <div class="context-bar">${this.escapeHtml(resp.detectedContextSummary)}</div>
  <div class="tabs">${tabsHtml}</div>

  <div class="concept-header">🎯 ${this.escapeHtml(cand.conceptTitle)}</div>

  <div class="code-box">${this.escapeHtml(cand.fullSuggestion)}${
            cand.nextLinePreview && cand.nextLinePreview !== cand.fullSuggestion
                ? `\n// Next step:\n${this.escapeHtml(cand.nextLinePreview)}`
                : ''
        }</div>

  <button class="primary-btn" onclick="insertCode()">✨ Insert Completion at Cursor (Tab)</button>

  <div class="card">
    <div class="card-title">💡 Progressive Learning Hint</div>
    <div>${this.escapeHtml(cand.progressiveHint)}</div>
  </div>

  ${deepLearningHtml}

  <script>
    const vscode = acquireVsCodeApi();
    function insertCode() { vscode.postMessage({ type: 'insert' }); }
    function selectCand(index) { vscode.postMessage({ type: 'selectCandidate', index }); }
    function forceExplain() { vscode.postMessage({ type: 'forceExplain' }); }
  </script>
</body>
</html>`;
    }
}
