"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const { CppBackendClient } = require("./cppBackendClient");
const { LearningCoachViewProvider } = require("./learningCoachView");

function activate(context) {
    const backend = new CppBackendClient(context.extensionPath);

    let latestResponse = null;
    let selectedCandidateIdx = 0;
    let lastActivityTimestamp = Date.now();
    let hintTimer = null;
    let stuckTimer = null;

    const codeLensEmitter = new vscode.EventEmitter();

    const statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBar.command = 'autocode.explainCurrentLine';
    statusBar.text = '$(mortar-board) AutoCode C++: Ready';
    statusBar.tooltip = 'Click to open the AutoCode C++ Learning Coach for the current line';
    statusBar.show();

    const insertCodeAtCursor = async (completionSuffix, nextLinePreview, triggerPrefix) => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            return;
        }
        const pos = editor.selection.active;
        const textToInsert = completionSuffix || (nextLinePreview ? '\n' + nextLinePreview : '');
        if (!textToInsert) {
            return;
        }
        await editor.edit((editBuilder) => {
            editBuilder.insert(pos, textToInsert);
        });
        backend.recordAccepted(triggerPrefix);
    };

    const coachViewProvider = new LearningCoachViewProvider(
        (suffix, nextLine, trigger) => {
            insertCodeAtCursor(suffix, nextLine, trigger);
        },
        (newIdx) => {
            selectedCandidateIdx = newIdx;
            vscode.commands.executeCommand('editor.action.inlineSuggest.trigger');
            codeLensEmitter.fire();
        }
    );

    context.subscriptions.push(
        vscode.window.registerWebviewViewProvider(
            LearningCoachViewProvider.viewType,
            coachViewProvider
        )
    );

    const updateStatusBar = (resp) => {
        if (!resp || resp.candidates.length === 0) {
            statusBar.text = '$(mortar-board) AutoCode C++: Watching';
            statusBar.backgroundColor = undefined;
            return;
        }
        const top = resp.candidates[Math.min(selectedCandidateIdx, resp.candidates.length - 1)];
        if (resp.stage === 'STUCK') {
            statusBar.text = `$(mortar-board) Stuck Coach: ${top.conceptTitle}`;
            statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
        } else if (resp.stage === 'PONDERING') {
            statusBar.text = `$(lightbulb) Hint: ${top.conceptTitle}`;
            statusBar.backgroundColor = undefined;
        } else {
            statusBar.text = `$(sparkle) AutoCode: ${top.conceptTitle}`;
            statusBar.backgroundColor = undefined;
        }
    };

    const runIdlePrediction = async (simulatedIdleMs, triggerInlineGhost) => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            return;
        }
        const doc = editor.document;
        if (doc.languageId !== 'cpp' && doc.languageId !== 'c') {
            return;
        }

        const pos = editor.selection.active;
        const resp = await backend.predict(
            doc.getText(),
            pos.line,
            pos.character,
            simulatedIdleMs,
            5
        );

        latestResponse = resp;
        if (resp && selectedCandidateIdx >= resp.candidates.length) {
            selectedCandidateIdx = 0;
        }

        updateStatusBar(resp);
        coachViewProvider.updatePrediction(resp, selectedCandidateIdx, simulatedIdleMs >= 4500);
        codeLensEmitter.fire();

        if (triggerInlineGhost && resp && resp.candidates.length > 0) {
            vscode.commands.executeCommand('editor.action.inlineSuggest.trigger');
        }
    };

    const resetIdleTimers = () => {
        lastActivityTimestamp = Date.now();
        if (hintTimer) {
            clearTimeout(hintTimer);
        }
        if (stuckTimer) {
            clearTimeout(stuckTimer);
        }

        const config = vscode.workspace.getConfiguration('autocode');
        const hintDelay = config.get('hintDelayMs', 2000);
        const stuckDelay = config.get('stuckDelayMs', 4500);

        hintTimer = setTimeout(() => {
            runIdlePrediction(hintDelay + 100, true);
        }, hintDelay);

        stuckTimer = setTimeout(() => {
            runIdlePrediction(stuckDelay + 100, true);
        }, stuckDelay);
    };

    const inlineProvider = {
        async provideInlineCompletionItems(document, position) {
            const idleMs = Date.now() - lastActivityTimestamp;
            const resp = await backend.predict(
                document.getText(),
                position.line,
                position.character,
                idleMs,
                5
            );

            latestResponse = resp;
            updateStatusBar(resp);
            coachViewProvider.updatePrediction(resp, selectedCandidateIdx);

            if (!resp || resp.candidates.length === 0) {
                return [];
            }

            const idx = Math.min(selectedCandidateIdx, resp.candidates.length - 1);
            const cand = resp.candidates[idx];
            const ghostText = cand.completionSuffix;
            if (!ghostText) {
                return [];
            }

            const item = new vscode.InlineCompletionItem(
                ghostText,
                new vscode.Range(position, position)
            );
            return [item];
        }
    };

    context.subscriptions.push(
        vscode.languages.registerInlineCompletionItemProvider(
            [{ language: 'cpp' }, { language: 'c' }],
            inlineProvider
        )
    );

    const codeLensProvider = {
        onDidChangeCodeLenses: codeLensEmitter.event,
        provideCodeLenses(document) {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.uri.toString() !== document.uri.toString()) {
                return [];
            }
            if (!latestResponse || latestResponse.candidates.length === 0) {
                return [];
            }
            if (latestResponse.stage === 'TYPING') {
                return [];
            }

            const line = editor.selection.active.line;
            const range = new vscode.Range(line, 0, line, 0);
            const cand =
                latestResponse.candidates[
                    Math.min(selectedCandidateIdx, latestResponse.candidates.length - 1)
                ];

            const explainLens = new vscode.CodeLens(range, {
                title: `💡 AutoCode Coach [${cand.conceptTitle}]: ${cand.progressiveHint}`,
                command: 'autocode.explainCurrentLine',
                tooltip: cand.detailedExplanation
            });

            const insertLens = new vscode.CodeLens(range, {
                title: `✨ Insert: ${cand.fullSuggestion.replace(/\n/g, ' ')}`,
                command: 'autocode.insertTopSuggestion',
                tooltip: 'Click to insert this C++ code suggestion at the cursor'
            });

            return [explainLens, insertLens];
        }
    };

    context.subscriptions.push(
        vscode.languages.registerCodeLensProvider(
            [{ language: 'cpp' }, { language: 'c' }],
            codeLensProvider
        )
    );

    const hoverProvider = {
        provideHover(document, position) {
            const editor = vscode.window.activeTextEditor;
            if (!editor || position.line !== editor.selection.active.line) {
                return null;
            }
            if (!latestResponse || latestResponse.candidates.length === 0) {
                return null;
            }
            const cand =
                latestResponse.candidates[
                    Math.min(selectedCandidateIdx, latestResponse.candidates.length - 1)
                ];

            const md = new vscode.MarkdownString();
            md.isTrusted = true;
            md.appendMarkdown(`### 🎓 AutoCode Coach: ${cand.conceptTitle}\n\n`);
            md.appendCodeblock(cand.fullSuggestion, 'cpp');
            md.appendMarkdown(`**💡 Hint:** ${cand.progressiveHint}\n\n`);
            md.appendMarkdown(`**📖 Why It Works:** ${cand.detailedExplanation}\n\n`);
            if (cand.complexityNote) {
                md.appendMarkdown(`**⏱ Complexity:** \`${cand.complexityNote}\`\n\n`);
            }
            if (cand.commonPitfall) {
                md.appendMarkdown(`**⚠️ Common Pitfall:** ${cand.commonPitfall}\n`);
            }
            return new vscode.Hover(md);
        }
    };

    context.subscriptions.push(
        vscode.languages.registerHoverProvider(
            [{ language: 'cpp' }, { language: 'c' }],
            hoverProvider
        )
    );

    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument((e) => {
            if (vscode.window.activeTextEditor?.document === e.document) {
                selectedCandidateIdx = 0;
                resetIdleTimers();
            }
        }),
        vscode.window.onDidChangeTextEditorSelection((e) => {
            if (e.textEditor === vscode.window.activeTextEditor) {
                resetIdleTimers();
            }
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('autocode.explainCurrentLine', async () => {
            await runIdlePrediction(5000, true);
            await vscode.commands.executeCommand('autocode.learningCoachView.focus');
        }),
        vscode.commands.registerCommand('autocode.insertTopSuggestion', async () => {
            if (!latestResponse || latestResponse.candidates.length === 0) {
                await runIdlePrediction(5000, false);
            }
            if (latestResponse && latestResponse.candidates.length > 0) {
                const cand =
                    latestResponse.candidates[
                        Math.min(selectedCandidateIdx, latestResponse.candidates.length - 1)
                    ];
                await insertCodeAtCursor(
                    cand.completionSuffix,
                    cand.nextLinePreview,
                    cand.triggerPrefix
                );
            }
        }),
        vscode.commands.registerCommand('autocode.cycleNextSuggestion', () => {
            if (latestResponse && latestResponse.candidates.length > 0) {
                selectedCandidateIdx =
                    (selectedCandidateIdx + 1) % latestResponse.candidates.length;
                coachViewProvider.updatePrediction(latestResponse, selectedCandidateIdx);
                codeLensEmitter.fire();
                vscode.commands.executeCommand('editor.action.inlineSuggest.trigger');
            }
        })
    );

    context.subscriptions.push(statusBar, {
        dispose: () => backend.dispose()
    });

    resetIdleTimers();
}
exports.activate = activate;

function deactivate() {}
exports.deactivate = deactivate;
