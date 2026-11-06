"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CppBackendClient = void 0;
const cp = require("child_process");
const fs = require("fs");
const path = require("path");
const readline = require("readline");
const vscode = require("vscode");

class CppBackendClient {
    constructor(extensionRoot) {
        this.proc = null;
        this.rl = null;
        this.pendingResolvers = [];
        this.extensionRoot = extensionRoot;
    }

    resolveBinaryPath() {
        const configPath = vscode.workspace.getConfiguration('autocode').get('binaryPath');
        if (configPath && fs.existsSync(configPath)) {
            return configPath;
        }
        const candidatePaths = [];
        candidatePaths.push(path.resolve(this.extensionRoot, '..', 'autocode-predictor'));
        candidatePaths.push(path.resolve(this.extensionRoot, '..', 'build', 'autocode-predictor'));
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

    ensureProcess() {
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
            this.rl.on('line', (line) => {
                const resolver = this.pendingResolvers.shift();
                if (!resolver) {
                    return;
                }
                try {
                    const parsed = JSON.parse(line);
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

    async predict(fileContent, cursorLine, cursorCol, idleMs, maxCandidates = 5) {
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
        return new Promise((resolve) => {
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
            this.proc.stdin.write(payload + '\n');
        });
    }

    recordAccepted(trigger) {
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

    dispose() {
        if (this.proc && !this.proc.killed) {
            this.proc.stdin.write(JSON.stringify({ action: 'quit' }) + '\n');
            this.proc.kill();
            this.proc = null;
        }
    }
}
exports.CppBackendClient = CppBackendClient;
