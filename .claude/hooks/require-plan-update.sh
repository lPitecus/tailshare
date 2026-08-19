#!/usr/bin/env bash
#
# Recusa um commit que mexe no código sem tocar no PLAN.md.
# A regra que ele defende está no CLAUDE.md: o plano é atualizado no mesmo
# commit do código. Escape: incluir --no-verify no git commit.
#
# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later

set -u

input=$(cat)
command=$(printf '%s' "$input" | jq -r '.tool_input.command // ""' 2>/dev/null || true)

# Só interessa git commit. Esta checagem fica aqui, e não no "if" do
# settings.json: aquele filtro casa por prefixo com o comando inteiro, então
# "git add x && git commit" nunca casaria e o gate passava batido.
case "$command" in
    *"git commit"*) ;;
    *) exit 0 ;;
esac

# Saída deliberada, quando o autor do commit assumir a decisão.
case "$command" in
    *--no-verify*) exit 0 ;;
esac

root=$(git rev-parse --show-toplevel 2>/dev/null) || exit 0
staged=$(git -C "$root" diff --cached --name-only 2>/dev/null) || exit 0

# Só interessa commit que mexe em código.
printf '%s\n' "$staged" | grep -qE '^(src|tests)/' || exit 0
# Plano já vai junto: nada a fazer.
printf '%s\n' "$staged" | grep -qx 'PLAN.md' && exit 0

cat <<'JSON'
{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"Este commit toca src/ ou tests/ mas nao inclui o PLAN.md, e a regra do projeto (CLAUDE.md) e que o plano seja atualizado no mesmo commit do codigo: marcar a fase concluida com o hash, registrar achados com a fonte, riscar pendencia resolvida, e manter a distincao entre verificado e suposto. Atualize o PLAN.md, adicione com git add e refaca o commit. Se esta mudanca realmente nao altera nada que o plano afirma (renomeacao mecanica, correcao de typo), diga isso ao usuario e refaca com --no-verify."}}
JSON
