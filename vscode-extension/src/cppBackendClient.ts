import * as cp from 'child_process';
import * as fs from 'fs';
import * as path from 'path';
import * as readline from 'readline';
import * as vscode from 'vscode';

export interface PredictionCandidate {
    triggerPrefix: string;
    completionSuffix: string;
    fullSuggestion: string;
    nextLinePreview: string;
    confidence: number;
    sourceEngine: string;
    conceptTitle: string;
    progressiveHint: string;
    detailedExplanation: string;
    complexityNote: string;
    commonPitfall: string;
}

export interface PredictionResponse {
    stage: 'TYPING' | 'PONDERING' | 'STUCK';
    stageCode: number;
    idleMs: number;
    detectedContextSummary: string;
    candidates: PredictionCandidate[];
}

export class CppBackendClient {
    private proc: cp.ChildProcessWithoutNullStreams | null = null;
    private rl: readline.Interface | null = null;
    private pendingResolvers: Array<(resp: PredictionResponse | null) => void> = [];
    private extensionRoot: string;

    constructor(extensionRoot: string) {
        this.extensionRoot = extensionRoot;
    }

    public resolveBinaryPath(): string | null {
        const configPath = vscode.workspace.getConfiguration('autocode').get<string>('binaryPath');
        if (configPath && fs.existsSync(configPath)) {
            return configPath;
        }

        const candidatePaths: string[] = [];

        // 1. Check parent directory of vscode-extension/
        candidatePaths.push(path.resolve(this.extensionRoot, '..', 'autocode-predictor'));
        candidatePaths.push(path.resolve(this.extensionRoot, '..', 'build', 'autocode-predictor'));

        // 2. Check open workspace folders
        if (vscode.workspace.workspaceFolders) {
            for (const folder of vscode.workspace.workspaceFolders) {
                candidatePaths.push(path.join(folder.uri.fsPath, 'autocode-predictor'));
                candidatePaths.push(path.join(folder.uri.fsPath, 'build', 'autocode-predictor'));
            }
        }

        for (const p of candidatePaths) {
            if (fs.existsSync(p)) {
                return p;
            }
        }
        return null;
    }

    private ensureProcess(): boolean {
        if (this.proc && !this.proc.killed) {
            return true;
        }

        const binPath = this.resolveBinaryPath();
        if (!binPath) {
            return false;
        }

        try {
            this.proc = cp.spawn(binPath, ['--server'], {
                stdio: ['pipe', 'pipe', 'pipe']
            });

            this.rl = readline.createInterface({
                input: this.proc.stdout,
                terminal: false
            });

            this.rl.on('line', (line: string) => {
                const resolver = this.pendingResolvers.shift();
                if (!resolver) {
                    return;
                }
                try {
                    const parsed = JSON.parse(line) as PredictionResponse;
                    resolver(parsed);
                } catch {
                    resolver(null);
                }
            });

            this.proc.on('exit', () => {
                this.proc = null;
                this.rl = null;
                while (this.pendingResolvers.length > 0) {
                    const r = this.pendingResolvers.shift();
                    if (r) {
                        r(null);
                    }
                }
            });

            return true;
        } catch {
            this.proc = null;
            return false;
        }
    }

    public async predict(
        fileContent: string,
        cursorLine: number,
        cursorCol: number,
        idleMs: number,
        maxCandidates: number = 5
    ): Promise<PredictionResponse | null> {
        if (!this.ensureProcess() || !this.proc) {
            return null;
        }

        const payload = JSON.stringify({
            action: 'predict',
            fileContent,
            cursorLine,
            cursorCol,
            idleMs,
            maxCandidates
        });

        return new Promise<PredictionResponse | null>((resolve) => {
            const timeout = setTimeout(() => {
                const idx = this.pendingResolvers.indexOf(resolve);
                if (idx !== -1) {
                    this.pendingResolvers.splice(idx, 1);
                    resolve(null);
                }
            }, 1500);

            this.pendingResolvers.push((res) => {
                clearTimeout(timeout);
                resolve(res);
            });

            this.proc!.stdin.write(payload + '\n');
        });
    }

    public recordAccepted(trigger: string): void {
        if (!this.ensureProcess() || !this.proc) {
            return;
        }
        const payload = JSON.stringify({
            action: 'accept',
            trigger
        });
        this.pendingResolvers.push(() => {});
        this.proc.stdin.write(payload + '\n');
    }

    public dispose(): void {
        if (this.proc && !this.proc.killed) {
            this.proc.stdin.write(JSON.stringify({ action: 'quit' }) + '\n');
            this.proc.kill();
            this.proc = null;
        }
    }
}
