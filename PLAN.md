# tailshare — Plano de Execução

Plugin nativo do KDE Plasma que adiciona um submenu **"Share via Tailscale"** ao
menu de contexto do Dolphin, listando os dispositivos da tailnet aptos a receber
arquivos e enviando via Taildrop (`tailscale file cp`).

**Estado:** Fases 0 e 1 concluídas (`4efe033`). Próximo passo: Fase 2 — `SendJob`
validado fora da UI pelo `tools/tailshare-probe`.

---

## 1. Decisões travadas

| Tema | Decisão |
|---|---|
| Integração | Plugin C++ `KAbstractFileItemActionPlugin` (KF6) — submenu dinâmico nativo |
| Escopo v1 | Somente **envio** (recebimento fica com o Tailscale) |
| Pastas | Compactadas automaticamente em **ZIP** antes do envio |
| Feedback | **KNotification** (Plasma nativo), sem janela própria |
| Dispositivos inaptos | Exibidos **desabilitados**, com o motivo no tooltip |
| Ordenação | **Online primeiro**, depois alfabética; sem estado persistido |
| Idioma | Inglês, com **KI18n** e catálogo pt-BR |
| Tailscale ausente/parado | Submenu **não aparece** |
| Licença | **GPL-2.0-or-later** |
| Testes | **QTest** no núcleo (parser, filtro, ordenação, montagem de comando) |
| Distribuição | **CMake install** + **PKGBUILD** (Arch/CachyOS) |
| Processo de envio | **Dentro do Dolphin** na v1 (assíncrono); helper desanexado fica para a v2 |
| Fechar o Dolphin durante o envio | Tentar um aviso *"A file is being sent via Tailscale. Close anyway?"*; se não for viável, notificar o cancelamento |
| Compactação | Sem pasta na seleção → arquivos vão crus; **com qualquer pasta → um ZIP único com tudo** |
| Ícone | Ícone do **tema** (Breeze), sem logo de marca registrada |
| Seleção remota (`sftp://`, `mtp://`) | Submenu **não aparece**; suporte fica para a v2 |
| Nomes | Projeto `tailshare`; plugin `tailshareitemaction.so`; lib `tailshare_core`; domínio i18n `tailshare` |

## 2. Fora de escopo (v1)

- Receber arquivos (`tailscale file get`) e daemon de recebimento
- Janela de configurações, favoritos, histórico de envios
- `tailscale funnel` / `serve` / links públicos
- Suporte a GNOME/Nautilus, Thunar ou outros gerenciadores
- Publicação em store.kde.org

### Backlog explícito da v2

- **Helper desanexado** (`tailshare-send`): envio sobrevive ao fechamento do Dolphin
- **Arquivos remotos**: baixar de `sftp://` / `mtp://` / `smb://` e então enviar
- Integração com o job tracker do Plasma (barra de progresso na bandeja)
- Receber arquivos (`tailscale file get`)

---

## 3. Restrições técnicas — verificadas na fonte oficial

Todas as afirmações abaixo foram conferidas contra o código-fonte do Tailscale
v1.102.2, o código-fonte do KIO/KF6, a documentação oficial do Taildrop, ou
medidas nesta máquina. Ver seção 10 para as fontes exatas.

### 3.1 Tailscale

| Fato | Fonte | Consequência |
|---|---|---|
| `tailscale file cp` **rejeita diretórios** com o erro literal `directories not supported` | `cmd/tailscale/cli/file.go:204` | Pastas precisam virar ZIP — confirmado, não era suposição |
| Aceita **N arquivos** num comando (`files, target := args[:len(args)-1], ...`) | `file.go:101` | Arquivos avulsos vão num único `cp` |
| `--name` é **incompatível com múltiplos arquivos** | `file.go:126-128` | Não usar `--name` no caminho de múltipla seleção |
| Taildrop só funciona **entre dispositivos do mesmo usuário**; não envia para nós *tagged* | docs oficiais Taildrop | Não vale implementar seleção de peers de terceiros |
| `status --json` traz `TaildropTarget` (enum) e `NoFileSharingReason` (texto) | `ipn/ipnstate/ipnstate.go:289-293` | Elegibilidade vem pronta do backend |
| Valores do enum: `Unknown`, `Available`, `NoNetmapAvailable`, `IpnStateNotRunning`, `MissingCap`, `Offline`, `NoPeerInfo`, `UnsupportedOS`, `NoPeerAPI`, `OwnedByOtherUser` | `ipnstate.go:343-356` | Mapa fechado de motivos → strings i18n nossas, sem repassar texto cru |
| **Offline não bloqueia o envio**: a CLI imprime `warning: <alvo> is reportedly offline; trying anyway` e tenta assim mesmo | `file.go:235-236` | ⚠️ Corrige o plano anterior — ver 3.3 |
| `--targets` usa `localClient.FileTargets()` e **inclui peers offline**, marcados com `offline` / `last seen` | `file.go:runCpTargets` | `status --json` continua sendo a melhor fonte (traz OS e o motivo estruturado) |
| `tailscale status --json` responde em **~5 ms** | medido aqui, 3 execuções | Consulta síncrona no menu é viável |

### 3.2 KDE Frameworks 6

| Fato | Fonte | Consequência |
|---|---|---|
| `actions()` é **síncrono, no processo do host**; o próprio header pede que seja assíncrono no KF7 para "um plugin ruim não impactar tanto o processo da aplicação" | `kabstractfileitemactionplugin.h` | Timeout duro de 300 ms na consulta ao Tailscale |
| Plugins são descobertos por `KPluginMetaData::findPlugins("kf6/kfileitemaction")` | `kfileitemactions.cpp:447` | ⚠️ **Não existe sycoca no caminho** — ver 3.3 |
| Filtro de MIME usa `mimeType.inherits(<declarado>)` | `kfileitemactions.cpp:450` | ⚠️ **`all/all` não funciona** aqui — ver 3.3 |
| Seleção mista de *arquivos* cai para `application/octet-stream` (`if (commonMimeType.isEmpty() && m_props.isFile())`) | `kfileitemactions.cpp:439-442` | Múltiplos arquivos de tipos diferentes continuam ativando o plugin |
| A instância do plugin é **cacheada e reusada** (`m_loadedPlugins`) | `kfileitemactions.cpp:463-471` | `actions()` não pode acumular estado entre chamadas |
| O usuário pode desabilitar o plugin pelo grupo `[Show]` do `kservicemenurc` | `kfileitemactions.cpp:444,459` | Documentar como desligar sem desinstalar |
| Metadados vão **embutidos no `.so`** via `K_PLUGIN_CLASS_WITH_JSON`; instalação em `${KDE_INSTALL_PLUGINDIR}/kf6/kfileitemaction` | plugin real do KDE Connect (`fileitemactionplugin/`) | Sem `.desktop`; o exemplo no header do KF6 está desatualizado (fala em `.desktop`, `KF5::KIOWidgets` e `kf5/...`) |
| `X-KDE-Show-In-Submenu: true` empurra o item para o submenu "Ações" | `kfileitemactions.cpp:476` | **Não** declarar — queremos o item no nível principal |
| `KDE_INSTALL_KNOTIFYRCDIR` = `share/knotifications6` | `KDEInstallDirs6.cmake:291` | Destino do `.notifyrc` |
| `KFileItemListProperties` expõe `isLocal()` e `isFile()` | `kfileitemlistproperties.cpp` | Base das regras de ocultação |

### 3.3 Correções ao plano anterior

Quatro afirmações da primeira versão estavam erradas ou imprecisas:

1. **"Enviar para peer offline falha"** — falso. A CLI só avisa e tenta mesmo
   assim (`file.go:235`). Manter o item desabilitado continua certo, mas por
   escolha de produto (evitar espera longa e fracasso silencioso), não por
   imposição da ferramenta. O `status --json` marca esses peers como
   `TaildropTarget: Offline`, então o critério de desabilitar é o enum, não um
   erro de envio.
2. **"`MimeTypes` cobrindo tudo com `all/all`"** — não funciona. Testei
   `QMimeType::inherits()` com Qt 6.11 nesta máquina: `all/all` e `all/allfiles`
   retornam **falso** para todos os tipos (esses coringas só valem para
   servicemenus `.desktop`, em outro trecho do KIO). O valor correto é
   **`["application/octet-stream", "inode/directory"]`**: todo arquivo regular
   herda de `application/octet-stream`, mas **`inode/directory` não herda** —
   sem declarar os dois, o submenu não aparece em pastas.
3. **"Rodar `kbuildsycoca6` após instalar"** — desnecessário no KF6. A descoberta
   é por varredura de `QT_PLUGIN_PATH`, sem banco sycoca. Basta reiniciar o
   Dolphin.
4. **"Fonte da lista"** — `--targets` parecia equivalente, mas devolve só IP,
   nome e um texto solto de status. `status --json` é estritamente superior:
   traz `OS`, `Online` e o enum estruturado. Decisão mantida, agora fundamentada.

### 3.4 Achados durante a implementação (Fase 1)

Verificados ao escrever o núcleo, todos contra o binário v1.102.2 e a tailnet
real desta máquina:

| Fato | Como foi verificado | Consequência |
|---|---|---|
| iOS e Android reportam `HostName: "localhost"` | `status --json` desta tailnet: 2 dos 4 peers | `Device::displayName()` cai para o primeiro label do `DNSName` (`iphone172`, `tcl-smart-tv-pro`) quando o host name é vazio ou `localhost` — sem isso o menu teria itens repetidos |
| `tailscale file cp -- <arquivos> <alvo>:` é aceito e o `--` é consumido | executado contra o binário real | `--` entra no argv: arquivo chamado `-v` continua sendo arquivo, sem depender de caminho absoluto |
| Valores do enum confirmados em dados reais: `1` = `Available`, `5` = `Offline` | peers online/offline da tailnet | A ordem documentada em 3.1 está correta; valor desconhecido degrada para `Unknown`, nunca crasha |
| `waitForStarted()` + `waitForFinished()` cobram o timeout **cada um** | leitura da API do `QProcess` | O orçamento de 300 ms é medido com `QElapsedTimer` sobre a chamada inteira, não por espera |
| Peer sem `DNSName` é inendereçável | decisão tomada ao escrever o parser; `sendTarget()` sai vazio nesse caso | O parser descarta esses peers em vez de deixar no menu um item que nunca enviaria |
| `qInfo()`/`qDebug()` saem silenciados neste ambiente | binário de teste sem saída até trocar por `QTextStream` | O `tailshare-probe` da Fase 2 deve escrever em `stdout` direto, ou o log não aparece |

**Suposição ainda não confirmada**, que vira item da Fase 5: assumimos que
`BackendState` `Stopped` e `NeedsLogin` vêm com `Peer: null` — as fixtures foram
escritas assim, mas isso **não** foi observado num `tailscale down` real. O
código não depende disso (o menu já some quando o estado não é `Running`), mas a
fixture precisa ser conferida contra a saída verdadeira.

Um ponto **não confirmado**, que vira item de teste da Fase 3: seleção mista de
*arquivo + pasta*. Nesse caso `commonMimeType` fica vazio e `isFile()` é falso,
então o fallback do KIO não se aplica; o casamento passa a depender de
`db.mimeTypeForName("")` — que empiricamente retorna `inherits("application/octet-stream") == true`
aqui, mas é comportamento do Qt em que prefiro não apostar sem testar no Dolphin real.

## 4. Arquitetura

Um artefato instalável (o plugin) sobre uma lib de núcleo testável. O envio roda
**dentro do processo do Dolphin**, de forma assíncrona.

```
[Dolphin]
   └─ tailshareitemaction.so           (plugin KIO)
        ├─ actions()      → consulta síncrona ≤300 ms, monta o submenu
        └─ SendJob (async) → ZIP (se houver pasta) → tailscale file cp → KNotification
                                   │
                                   └── usa libtailshare_core (estática, sem UI)
```

### 4.1 `libtailshare_core` (estática, sem dependência de UI) — ✅ implementada

Depende só de `Qt6::Core` e `KF6::I18n`. Cada arquivo tem uma responsabilidade:

| Arquivo | O que faz |
|---|---|
| `device.h/cpp` | `Device` (`hostName`, `dnsName`, `os`, `online`, `taildropTarget`, `noFileSharingReason`) + enum `TaildropTarget` e `taildropTargetFromValue()`. Métodos: `displayName()`, `sendTarget()`, `canReceiveFiles()` |
| `statusparser.h/cpp` | `parseStatus(QByteArray)` → `Status{valid, error, backendState, devices}`. **Separado do cliente de propósito**: é o que permite testar todo o parsing por fixture, sem processo nem tailnet |
| `devices.h/cpp` | `Devices::eligible()` e `Devices::sorted()`. `sorted()` usa `QCollator` (case-insensitive, ciente de locale); `eligible()` preserva a ordem de entrada, então quem quiser as duas coisas compõe as chamadas |
| `taildropreason.h/cpp` | `taildropReasonText()` — enum → string i18n nossa (ver 3.1); o texto cru do backend nunca vai para a UI |
| `tailscaleclient.h/cpp` | Executa o comando e devolve `Status`. `program`/`arguments`/`timeout` são injetáveis — é assim que os testes trocam o tailscale por `/bin/cat` lendo fixture ou por um `sh -c 'sleep 5'` para exercitar o timeout |
| `sendplan.h/cpp` | `SendPlan::build(paths, device, now)` → `needsArchive()`, `archiveFileName()`, `filesToSend(zip)`, `commandArguments(zip)`. O `now` é injetável para o nome de fallback ser testável |

Sem `system()`, sem string de shell: `QProcess` com lista de argumentos, e `--`
antes dos caminhos (ver 3.4), então nome com espaço, acento, aspas ou traço
inicial fica seguro por construção. `commandArguments()` devolve **vazio** quando
o plano exige ZIP e ninguém passou o caminho dele — não existe comando legítimo
nesse estado.

Nome do ZIP único (implementado como planejado): uma pasta sozinha →
`<nome-da-pasta>.zip`; seleção com vários itens → `<nome-da-pasta-pai>.zip`,
caindo para `tailshare-<AAAAMMDD-HHMMSS>.zip` quando o pai não der um nome útil.

### 4.2 `tailshareitemaction.so` (o plugin)

Fluxo de `actions()` — tudo síncrono e barato:

1. `KFileItemListProperties::isLocal()` falso → retorna vazio (remoto é v2).
2. `tailscale` não encontrado no PATH → vazio.
3. `status --json` (timeout 300 ms) falha, ou `BackendState != "Running"` → vazio.
4. Nenhum peer na tailnet → vazio.
5. Monta `QMenu` "Share via Tailscale" com ícone do tema; um `QAction` por
   dispositivo, ícone conforme `OS` (`computer-laptop`, `smartphone`, …).
   Habilitado somente quando `TaildropTarget == Available`; os demais ficam
   `setEnabled(false)` com o motivo traduzido no tooltip, derivado do enum
   (não do texto cru de `NoFileSharingReason`).

Metadados JSON (`MimeTypes`): `["application/octet-stream", "inode/directory"]` —
os dois são obrigatórios (ver 3.3). Sem `X-KDE-Show-In-Submenu`, para o item
ficar no nível principal do menu. Como o KIO reusa a instância do plugin,
`actions()` não guarda estado entre chamadas.

### 4.3 `SendJob` — o envio, dentro do Dolphin

Disparado pelo clique, `QObject` filho do plugin, 100% assíncrono. O menu fecha
na hora e o Dolphin nunca bloqueia.

1. Notificação persistente: *"Sending 3 files to pixel-8…"*.
2. Se houver pasta na seleção: `KZip` → `$XDG_RUNTIME_DIR/tailshare-XXXX/<nome>.zip`,
   feito **numa `QThread`** para não travar a interface do Dolphin. Notificação
   passa a *"Compressing…"*. Falha (espaço, permissão) → notificação de erro e aborta.
3. `tailscale file cp <arquivos> <dnsName>:` via `QProcess` assíncrono.
4. Sucesso → *"Sent to pixel-8"*. Erro → notificação com o **stderr do tailscale**
   (é a mensagem mais útil: sem permissão, peer inválido, etc.).
5. `QTemporaryDir` limpa o ZIP sempre, inclusive em erro.

**Fechar o Dolphin com envio em andamento.** Enquanto houver `SendJob` ativo, o
plugin instala um event filter de `QEvent::Close` na janela de topo recebida em
`parentWidget` e mostra *"A file is being sent via Tailscale. Close anyway?"*.
Isso **não é API suportada** — um plugin de menu de contexto não tem um caminho
oficial para vetar o fechamento da janela do host. Vira um spike de meia hora na
Fase 3; se não funcionar de forma confiável, o plano B decidido é aceitar o
cancelamento e emitir *"Transfer canceled: Dolphin was closed"*, deixando a
solução real (helper desanexado) para a v2.

---

## 5. Estrutura do repositório

```
tailshare/
├── CMakeLists.txt
├── LICENSE                       # GPL-2.0-or-later
├── README.md
├── PLAN.md
├── src/
│   ├── core/                     # ✅ libtailshare_core (7 pares .h/.cpp)
│   └── plugin/                   # tailshareitemaction.cpp, sendjob.cpp, .json
├── tools/
│   └── tailshare-probe/          # binário de dev, NÃO instalado (ver Fase 2)
├── tests/                        # ✅ 5 binários QTest + CMakeLists
│   ├── fixtures/                 # ✅ 8 JSONs de status (anonimizados)
│   └── *test.cpp
├── po/                           # pt_BR.po
├── data/
│   └── tailshare.notifyrc        # eventos de notificação
└── packaging/
    └── PKGBUILD
```

O caminho das fixtures chega aos testes por `target_compile_definitions`
(`FIXTURE_DIR`), então `ctest` roda de qualquer diretório.

---

## 6. Fases

### ✅ Fase 0 — Fundação (concluída, `926c8ff`)
`extra-cmake-modules` instalado; CMake raiz com ECM, `LICENSE`, `.gitignore`,
`README` com pré-requisitos.
**Entregue:** `cmake -B build && cmake --build build` compila.

### ✅ Fase 1 — Núcleo + testes (concluída, `4efe033`)
Núcleo completo (ver 4.1) e **60 casos QTest em 5 binários** sobre 8 fixtures:
tailnet normal, peer offline, peer com `NoFileSharingReason`, backend
`NeedsLogin` e `Stopped`, JSON malformado, tailnet vazia, JSON com campos
ausentes e valor de enum inexistente. `TailscaleClient` é testado com fixture via
`/bin/cat`, timeout via `sleep 5` (verifica retorno em <2 s), saída não-zero com
stderr repassado, e saída que não é JSON. `SendPlan` cobre só arquivos, uma
pasta, várias pastas, pasta + arquivos, caminho relativo, caminho inexistente,
seleção vazia, dispositivo sem `DNSName`, e nomes com espaço, acento e traço
inicial.
**Entregue:** `ctest` passa 100%; validado também contra a tailnet real por um
binário de smoke descartável (não versionado).

### Fase 2 — Envio validado fora da UI
`SendJob` completo (ZIP em thread, envio, notificações, limpeza) exercitado por
`tools/tailshare-probe`: um binário mínimo de linha de comando, **não instalado
pelo pacote**, que existe só para validar o caminho de envio antes de haver menu.
Ele vira a semente do helper desanexado da v2.
**Pronto quando:** enviar arquivo, pasta e seleção mista para um dispositivo real
funciona, e as notificações (comprimindo / enviando / ok / erro) aparecem certas.

### Fase 3 — Plugin do menu
Plugin, metadados JSON (`application/octet-stream` + `inode/directory`), regras de
ocultação, ícones por SO, itens desabilitados com tooltip, ligação com o `SendJob`.
Inclui o **spike do aviso de fechamento** (4.3): event filter na janela pai; se
não for confiável, cai para a notificação de cancelamento.
Verificar no Dolphin real os cinco casos de casamento de MIME: arquivo único,
vários arquivos do mesmo tipo, vários de tipos diferentes, só pastas, e
**arquivo + pasta** (o caso não confirmado da seção 3.3).
**Pronto quando:** o submenu aparece nos cinco casos, o envio funciona ponta a
ponta, o menu não trava com Tailscale parado (`tailscale down`), e o
comportamento ao fechar o Dolphin está definido e documentado.

### Fase 4 — i18n e empacotamento
`i18n()` em todas as strings, `pt_BR.po`, `.notifyrc` em `KDE_INSTALL_KNOTIFYRCDIR`,
`PKGBUILD`, instruções de instalação/desinstalação no README (incluindo como
desligar o plugin pelo grupo `[Show]` do `kservicemenurc`).
**Pronto quando:** `makepkg -si` instala, o plugin carrega após reiniciar o
Dolphin (sem `kbuildsycoca6` — ver 3.3), e a UI aparece em pt-BR com locale pt_BR.

### Fase 5 — QA manual
Roteiro fechado: arquivo único, múltiplos arquivos, pasta, pasta grande, nome com
espaços e acentos, peer offline, peer sem Taildrop, tailnet só com self,
`tailscale down` no meio do envio, fechar o Dolphin durante o envio, seleção em
`sftp://`. Inclui conferir a saída real de `status --json` com `tailscale down`
e sem login, para corrigir as fixtures `stopped.json` e `needs-login.json` (3.4).
**Pronto quando:** cada caso tem comportamento observado e documentado no README.

---

## 7. Dependências

**Build:** `extra-cmake-modules` (instalado), `cmake`, `g++`, Qt 6.11 — mais
`Qt6::Test` para a suíte (`BUILD_TESTING`, ligado por padrão via ECM).
**KF6 (já presentes):** KIO 6.28, KCoreAddons, KI18n, KNotifications, KArchive, KWidgetsAddons.
**Runtime:** `tailscale` ≥ 1.102 no PATH; Plasma 6, Dolphin 26.04.

---

## 8. Riscos

| Risco | Mitigação |
|---|---|
| `actions()` lento degrada o menu de contexto inteiro do Dolphin | Timeout de 300 ms; medir o custo real do submenu na Fase 3 e registrar no README |
| Formato do `status --json` muda entre versões (o próprio `--help` avisa) | Parser tolerante: campo faltando vira valor seguro, nunca crash; fixtures versionadas |
| Zipar pasta grande consome disco e demora | Notificação *"Compressing…"* e compactação em `QThread`, para a interface do Dolphin não congelar |
| Envio morre ao fechar o Dolphin (decisão da v1) | Spike do aviso de fechamento na Fase 3; plano B é notificar o cancelamento; solução definitiva é o helper da v2 |
| Event filter na janela do host é API não suportada e pode quebrar entre versões do Dolphin | Isolado num único ponto do código, com fallback já decidido; nunca bloqueia o fechamento se falhar |
| Peer aparece online mas o envio falha | Erro real do tailscale repassado íntegro na notificação |
| Envio a peer offline fica pendurado (a CLI tenta assim mesmo, `file.go:235`) | Itens offline já vêm desabilitados; o `SendJob` ainda assim aplica timeout e reporta |
| Plugin não aparece por MIME não casado | Casos de MIME viram checklist explícito da Fase 3 |
| Plugin novo não é detectado | Documentar reinício do Dolphin no README (sycoca não se aplica) |
| Usuário desabilita o plugin sem querer | Documentar o grupo `[Show]` do `kservicemenurc` no README |

---

## 9. Pontos ainda em aberto

As seis perguntas da versão anterior foram todas respondidas e viraram linhas da
seção 1. Restam dois pontos:

1. **Aviso ao fechar o Dolphin** — viabilidade só se sabe no spike da Fase 3
   (4.3). Plano B já decidido.
2. **Fixtures de `Stopped` e `NeedsLogin`** — escritas com `Peer: null` por
   suposição, ainda não conferidas contra um `tailscale down` real (ver 3.4).

Resolvidos desde a versão anterior:

- ~~**Nome do ZIP único** em seleção mista~~ — implementado e testado conforme
  a proposta de 4.1.
- ~~**Renomear o diretório e o remote**~~ — feito: o diretório é `tailshare` e o
  remote é `git@github.com:lPitecus/tailshare.git`.

---

## 10. Fontes da verificação

**Tailscale** (código da própria versão instalada, v1.102.2):
- `cmd/tailscale/cli/file.go` — recusa de diretórios (l. 204), múltiplos arquivos
  (l. 101), `--name` (l. 126), aviso de offline (l. 235), motivos de recusa
  (l. 494-517), `runCpTargets`
- `ipn/ipnstate/ipnstate.go` — `TaildropTarget` / `NoFileSharingReason` (l. 289-293),
  enum `TaildropTargetStatus` (l. 343-356)
- Documentação oficial do Taildrop — restrição a dispositivos do mesmo usuário e
  a nós *tagged*
- `tailscale file cp --help`, `tailscale status --json` e medição de latência
  nesta máquina

**KDE Frameworks 6** (KIO 6.28 instalado + fonte upstream):
- `/usr/include/KF6/KIOWidgets/kabstractfileitemactionplugin.h` — contrato de
  `actions()` e o TODO de KF7
- `frameworks/kio/src/widgets/kfileitemactions.cpp` — descoberta de plugins,
  filtro de MIME, fallback de seleção mista, cache de instância, grupo `[Show]`,
  `X-KDE-Show-In-Submenu`
- `frameworks/kio/src/core/kfileitemlistproperties.cpp` — `isLocal()`, `isFile()`,
  cálculo do MIME comum
- `network/kdeconnect-kde/fileitemactionplugin/` — plugin KF6 real: `CMakeLists.txt`
  (destino de instalação) e `kdeconnectsendfile.json` (formato dos metadados)
- `extra-cmake-modules/kde-modules/KDEInstallDirs6.cmake` — `KNOTIFYRCDIR`, `PLUGINDIR`
- Teste compilado localmente com Qt 6.11 para `QMimeType::inherits()` com
  `all/all`, `all/allfiles`, `application/octet-stream` e `inode/directory`
