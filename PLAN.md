# tailshare — Plano de Execução

Plugin nativo do KDE Plasma que adiciona um submenu **"Share via Tailscale"** ao
menu de contexto do Dolphin, listando os dispositivos da tailnet aptos a receber
arquivos e enviando via Taildrop (`tailscale file cp`).

**Estado:** Fases 0 a 3 concluídas (`72cf376`). O plugin funciona no Dolphin real: submenu,
envio, itens desabilitados, notificações e aviso ao fechar a janela.
Próximo passo: Fase 4 — i18n (`pt_BR.po`) e empacotamento (`PKGBUILD`).

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
2. **"`MimeTypes` cobrindo tudo com `all/all`"** — não funciona (e a chave
   ainda fica no lugar errado neste texto: ver 3.6). Testei
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

**Suposição desmentida na Fase 5** (ver 3.8): assumíamos que `BackendState`
`Stopped` e `NeedsLogin` vinham com `Peer: null`, e as fixtures foram escritas
assim. Um `tailscale down` real mostrou o contrário para `Stopped` — a tailnet
inteira continua listada. A fixture `stopped.json` foi refeita a partir da
captura verdadeira. Como já se previa aqui, o código não dependia disso: o menu
some porque o estado não é `Running`, e não porque a lista está vazia.

Um ponto **não confirmado**, que vira item de teste da Fase 3: seleção mista de
*arquivo + pasta*. Nesse caso `commonMimeType` fica vazio e `isFile()` é falso,
então o fallback do KIO não se aplica; o casamento passa a depender de
`db.mimeTypeForName("")` — que empiricamente retorna `inherits("application/octet-stream") == true`
aqui, mas é comportamento do Qt em que prefiro não apostar sem testar no Dolphin real.

### 3.5 Achados durante a implementação (Fase 2)

Verificados ao escrever o caminho de envio, contra o tailscale v1.102.2, os
headers do KF6 6.28 instalados e o barramento de notificações desta sessão do
Plasma:

| Fato | Como foi verificado | Consequência |
|---|---|---|
| `tailscale file cp` recusa o envio quando o usuário não é *operator*: `Access denied: file access denied` seguido de `To not require root, use 'sudo tailscale set --operator=$USER' once.` | `tailshare-probe` contra `home-tuzin` nesta máquina | Não é falha do plugin, é configuração do tailscaled — **virou pré-requisito de runtime no README**. Confirma também a decisão de repassar o stderr íntegro: a própria mensagem ensina a correção |
| `KF6::Notifications` arrasta `Qt6::Gui` (`INTERFACE_LINK_LIBRARIES "Qt6::Gui;Qt6::DBus"`) | `/usr/lib/cmake/KF6Notifications/KF6NotificationsTargets.cmake:72` | O envio ficou em `tailshare_send`, sem UI, e a notificação em `tailshare_notify`. Testes e o modo padrão do probe rodam sem display |
| `KNotificationAction` não tem header próprio: é declarado dentro de `knotification.h` | erro de compilação `KNotificationAction: No such file or directory`, depois `knotification.h:30` | `#include <KNotification>` basta |
| Chamar `sendEvent()` duas vezes na mesma `KNotification` cria **dois** popups, não atualiza o primeiro | `dbus-monitor --session` em `org.freedesktop.Notifications`: a segunda chamada saiu com `replaces_id = 0` | Enviar uma vez só; depois disso os setters (`setTitle`, `setText`, `setIconName`) é que atualizam |
| A atualização só sai **depois** que o servidor devolve o id, e é assíncrona | mesma medição: num job de 18 ms a ordem no barramento foi `Compressing` (novo) → `Send failed` (novo) → `Sending files` com `replaces_id=11` — o popup de progresso ressuscitou **depois** do resultado | O `SendNotifier` só levanta o progresso depois de **400 ms** de transferência. Remedido: job de 18 ms gera **um** `Notify` (o resultado); job de 3 s gera `Notify` de progresso, `CloseNotification` e `Notify` final, nessa ordem |
| `$XDG_RUNTIME_DIR` existe e é usável aqui (`/run/user/1000`) | caminho do ZIP na mensagem de erro do tailscale: `/run/user/1000/tailshare-dhpCgT/pasta de teste.zip` | O ZIP temporário nunca passa por um `/tmp` compartilhado |
| KArchive não expõe progresso nem interrupção — `addLocalDirectory()` é uma chamada só, que recursa sozinha | `karchive.h:138-153` | Cancelar só é percebido **entre** os itens de topo; está documentado na API do `Archiver`, e uma pasta enorme ainda termina antes de a flag ser vista |
| Dois itens de mesmo nome em pastas diferentes colidiriam dentro do ZIP | teste `doesNotLoseItemsThatShareAName` | O `Archiver` renomeia o segundo para `notes-2.txt` em vez de perder um dos dois silenciosamente |
| `QTimer` de 1000 ms disparou aos 950 ms medidos pelo `QElapsedTimer` do probe | `--program <script de 3 s> --timeout 1000` | Timeout é orçamento aproximado, não garantia; irrelevante aqui, mas não usar `QTimer` para medir prazo curto |

**Correção à seção 8**: o `SendJob` aceita um timeout, mas ele vem **desligado**
(`timeout() == 0`). Um envio legítimo de vários gigabytes e um envio pendurado
num peer que caiu são indistinguíveis daqui, e matar o primeiro para pegar o
segundo é o pior lado do trade. A saída é o `cancel()` — exposto como ação
*Cancel* na notificação persistente e como Ctrl+C no probe — mais o fato de o
menu já não oferecer dispositivo que o Taildrop diz inalcançável.

### 3.6 Achados durante a implementação (Fase 3)

| Fato | Como foi verificado | Consequência |
|---|---|---|
| **`MimeTypes` vai dentro do objeto `KPlugin`, não na raiz do JSON** | com a chave na raiz, `KPluginMetaData::mimeTypes()` devolve **lista vazia** e os cinco casos de casamento falham; movida para dentro de `KPlugin`, os cinco passam (`plugintest`) | ⚠️ Corrige 3.2: o exemplo do KDE Connect foi lido como se a chave fosse de raiz. Sem isso o submenu nunca apareceria, e o erro é silencioso |
| `actions()` custa **17 ms** no pior de 5 execuções, com o tailscale real | `plugintest measuresWhatTheContextMenuPays` com `TAILSHARE_REAL_TAILSCALE=/usr/bin/tailscale` | O orçamento de 300 ms tem folga de mais de 15×; risco da seção 8 controlado |
| Seleção **arquivo + pasta** casa | `plugintest kioMatchesEverySelectionShape(file and folder)`: `mimeType()` vem vazio, `isFile()` é falso, e `QMimeDatabase::mimeTypeForName("")` ainda assim herda de `application/octet-stream` | Resolve a dúvida de 3.3. Ressalva: o teste **replica** a regra do KIO lida no fonte, não executa o Dolphin — a confirmação final é o QA manual |
| `QMenu` não mostra tooltip de item nenhum sem `setToolTipsVisible(true)` | API do Qt; sem a chamada, o motivo do item desabilitado não teria como aparecer | Chamada explícita no plugin, com comentário |
| `KPluginFactory` instancia pelo construtor `(QObject *, const QVariantList &)` | `plugintest` carrega o `.so` pelo mesmo caminho do KIO e obtém a instância | Assinatura fixada; o exemplo do header (`.desktop`, KF5) continua desatualizado |
| Rodar o plugin do *build tree* depende de `QT_PLUGIN_PATH` **absoluto** e de nenhum Dolphin vivo | primeira tentativa de QA não mostrou nada; `KPluginMetaData::findPlugins("kf6/kfileitemaction")` acha o plugin quando a variável está certa, e `/proc/<pid>/environ` do Dolphin mostrou que ela não tinha chegado | Um caminho errado falha **em silêncio**, e um Dolphin já rodando recebe o comando por DBus sem herdar o ambiente. Documentado no README |
| A entrada cai no **nível principal** do menu que o `KFileItemActions` monta | `plugintest reachesTheMenuKioBuilds`, que percorre o caminho do KIO lendo o `kservicemenurc` real desta máquina | Confirma que não declarar `X-KDE-Show-In-Submenu` faz o esperado |
| O executável do tailscale precisava ser injetável para testar o menu | sem isso o `plugintest` dependeria da tailnet desta máquina | `TailscaleClient::findExecutable()` passou a honrar `$TAILSHARE_TAILSCALE`, o que também serve a instalação fora do PATH |

### 3.7 Achados durante a implementação (Fase 4)

| Fato | Como foi verificado | Consequência |
|---|---|---|
| `ki18n_install(po)` compila os catálogos para `<build>/locale`, e **não** para `<build>/share/locale` | `/usr/lib/cmake/KF6I18n/KF6I18nMacros.cmake`, l. 136–167: `COPY_TO=${CMAKE_CURRENT_BINARY_DIR}/${dirname}`, com `dirname` = último componente de `KDE_INSTALL_LOCALEDIR` | O `KLocalizedString` procura em `XDG_DATA_DIRS/locale`, então o build tree ficaria sem tradução. Alvo extra no CMake raiz espelha para `build/share/locale`, e um único `XDG_DATA_DIRS` passa a cobrir notificação **e** tradução |
| Tradução funciona a partir do build tree, sem instalar nada | `LANGUAGE=pt_BR LANG=pt_BR.UTF-8 XDG_DATA_DIRS="$PWD/build/share:…" ./build/bin/tailshare-probe --list` imprimiu `Este dispositivo está offline.` para os dispositivos offline da tailnet real | O fluxo "experimentar sem instalar" do README também vale para o pt-BR |
| `KPluginMetaData::name()` lê a chave `Name[pt_BR]` do JSON **pelo locale do processo** | `LANG=pt_BR.UTF-8 ./build/bin/plugintest theBinaryCarriesItsMetadata` passou a **falhar**, com `data.name()` devolvendo `Compartilhar pelo Tailscale` | Confirma o mecanismo (`KJsonUtils::readTranslatedString`, `kjsonutils.h`) e revela que o teste antigo dependia do locale de quem roda: agora ele fixa `QLocale::setDefault(QLocale::c())` e tem um caso irmão que checa as duas línguas. `ctest` dá 100% com `LANG=en_US` e com `LANG=pt_BR` |
| O `msgcmp` reprova string nova sem tradução | uma `i18n()` temporária foi acrescentada a `taildropreason.cpp` e `tests/check-translations.sh` saiu com código 1: *"this message is used but not defined"* | O `translationstest` é guarda de verdade, não decoração: strings novas não passam despercebidas |
| O `ctest` já rodava um `appstreamtest` que ninguém escreveu | apareceu em `ctest -N`; vem de `KDECMakeSettings.cmake:177`, que adiciona o teste quando acha o `appstreamcli` | Passa vazio porque o projeto não instala metainfo AppStream. Publicar metainfo continua fora do escopo da v1 |
| O `.pot` não é versionado | `.gitignore` já ignorava `*.pot` desde a Fase 0 | Segue a convenção do KDE: o template é gerado pelo `Messages.sh`, e o `translationstest` extrai o seu próprio em diretório temporário em vez de confiar num arquivo no repo |
| O `PKGBUILD` não pode clonar por **https**: o repositório é privado | `makepkg` com `source=("…git+https://github.com/lPitecus/tailshare.git")` parou em `fatal: could not read Username for 'https://github.com'`; `gh repo view` confirma `"isPrivate": true` | A fonte do pacote é `git+ssh://git@github.com/…`, igual ao remote do repo. Trocar por https no dia em que o projeto for público — está anotado no próprio `PKGBUILD` |
| O `makepkg` **reescreve a linha `pkgver=`** do PKGBUILD no diretório de origem | o arquivo versionado saiu do commit `af175e4` já com `0.1.0.r14.af175e4`, valor que o `pkgver()` calculou naquela execução | Comportamento documentado (`PKGBUILD(5)`, *"The pkgver variable can be automatically updated by providing a pkgver() function"*) e normal em pacote VCS. Consequência prática: todo `makepkg` suja o repositório em uma linha — é para commitar junto, não para reverter |
| `gettext` sobrava no `makedepends` | `pacman -Si base-devel` lista `gettext` entre as dependências do meta-pacote, e a wiki do Arch diz que o `base-devel` é presumido instalado e seus membros não devem entrar no `makedepends` | Removido, com comentário no PKGBUILD explicando de onde vem o `msgfmt`. `git` e `cmake` continuam: nenhum dos dois está no `base-devel`, e as diretrizes de VCS exigem o `git` explícito |
| O `makepkg` deixa um clone *bare* em `packaging/tailshare/` | apareceu como não rastreado no `git status` depois do primeiro `makepkg` | Entrou no `.gitignore`. Comentário de fim de linha **não** funciona em `.gitignore`: a primeira tentativa virou parte do padrão e o diretório continuou aparecendo |

### 3.8 Achados durante a implementação (Fase 5)

| Fato | Como foi verificado | Consequência |
|---|---|---|
| Com o backend **parado**, o `status --json` continua listando **a tailnet inteira**, e não `Peer: null` | `tailscale down` nesta máquina, `tailscale status --json` capturado em seguida e `tailscale up` para restaurar: 4 peers presentes | ⚠️ Corrige 3.4 e a fixture `stopped.json`, que foi refeita com a forma real |
| O que muda no estado parado é o `TaildropTarget` de **cada peer**, que vira `3` (`IpnStateNotRunning`) | mesma captura: os 4 peers vêm com `TaildropTarget: 3`, contra `1` (online) e `5` (offline) na captura feita segundos antes com a rede no ar | O motivo mostrado ao usuário seria *"Tailscale is not running."*, coerente com o estado — mas o menu nem chega a aparecer |
| O `Online` dos peers **congela** no último valor conhecido quando o backend para | na captura parada, `iphone-9` e `home-nas` seguem `Online: true` | Confirma que `Online` sozinho não serve de critério; quem decide é o `TaildropTarget`, como o parser já fazia |
| `Self.TaildropTarget` é `0` (`Unknown`) nos dois estados | capturas com a rede no ar e parada | A fixture antiga dizia `3` para o `Self`; era palpite. O `Self` não entra na lista de destinos de qualquer forma |
| O `statusparsertest` afirmava `devices.isEmpty()` para `Stopped` | com a fixture verdadeira o teste **falhou** | A afirmação era da suposição, não do comportamento. Agora o teste espera os 4 dispositivos e verifica o que de fato importa: nenhum deles pode receber, e todos reportam `IpnStateNotRunning` |
| Backend derrubado **no meio de um envio** não gera erro: o `tailscale file cp` **bloqueia e retoma** quando a rede volta | reproduzido nesta máquina com o `tailshare-probe` e 300 MB para o `home-tuzin`: envio às 18:47:53, `tailscale down` às 18:47:56 (backend `Stopped`), processo ainda vivo 15 s depois, `tailscale up` às 18:48:11, e `succeeded` às 18:48:24 — 31,5 s no total, dos quais ~15 s de rede no chão. O usuário tinha observado o mesmo pelo menu do Dolphin | Confirma, com evidência, a decisão de 3.5 de deixar o timeout do `SendJob` **desligado**: um timeout curto teria matado uma transferência que ia terminar bem. Também explica o que o usuário vê — a notificação *Enviando* fica parada, sem erro, até a rede voltar |
| `NeedsLogin` **também** não vem com `Self`/`Peer` nulos | `tailscale logout` real nesta máquina: `Self` é objeto com `DNSName` vazio e `TaildropTarget: 0`; `Peer` traz **uma** entrada residual do engine, com `HostName`, `DNSName` e `OS` vazios, `InNetworkMap: false`, `InEngine: true` e os contadores `RxBytes`/`TxBytes` da última transferência | ⚠️ Segunda fixture desmentida. A `needs-login.json` foi refeita com a forma real |
| O `AuthURL` vem **vazio** logo após o `logout` | mesma captura: `"AuthURL": ""`, ao contrário da fixture antiga, que trazia uma URL de login | A URL provavelmente aparece com um `tailscale up` pendente de autenticação, mas **isso não foi observado** — a fixture registra só o estado que foi visto |
| A lista de dispositivos continua vazia em `NeedsLogin`, por outro motivo | a suíte passa sem alteração com a fixture nova | Antes era vazia porque `Peer` era `null`; agora é vazia porque o peer residual não tem `DNSName` e o parser o descarta, que é a regra de 3.4 — o comportamento certo, pelo motivo certo |
| O submenu some por causa do **estado**, não da lista vazia | o `plugintest staysAwayWhenTheBackendIsNotRunning(stopped)` continua passando com a fixture nova, que tem 4 dispositivos | Antes o teste não sabia distinguir as duas causas; agora sabe |

### 3.9 Achados das verificações antes da publicação (Fase 6)

| Fato | Como foi verificado | Consequência |
|---|---|---|
| O projeto compila **sem um aviso sequer** também no clang | árvore paralela `build-clang` com `CC=clang CXX=clang++`, build completo: 0 avisos | Segundo compilador concorda com o gcc; os avisos que o `KDECompilerSettings` liga já estavam sendo respeitados |
| `clang-tidy` não acha código morto nenhum **dentro** de um arquivo | `misc-unused-*`, `readability-redundant-declaration` e `clang-diagnostic-unused*` sobre os 13 `.cpp` de `src/`: 0 avisos | O que sobra é o que nenhuma dessas checagens vê: função pública que ninguém chama |
| O `compile_commands.json` do gcc **não serve** para o clang-tidy | `error: unknown argument: '-mno-direct-extern-access'` | Daí a árvore `build-clang` separada, que de quebra dá o segundo compilador |
| Quatro funções mortas, achadas por análise de símbolos | `nm --defined-only` × `nm --undefined-only` sobre os 37 objetos, filtrando moc e templates do Qt: 18 candidatos, reduzidos a 4 conferindo cada um no código-fonte | `SendNotifier::delay()`/`setDelay()` (o atraso da notificação nunca foi configurado por ninguém — é sempre `DefaultDelayMs`), `CloseGuard::activeJobs()` (quem conta é o `pruneAndCount()` privado) e `SendJob::program()`. Removidas |
| O `nm` sozinho não decide nada | ele não enxerga chamada dentro da mesma unidade de compilação, então acusou métodos privados e *slots* que estão vivos | A lista dele é ponto de partida; a decisão veio de `grep` no fonte, um a um |
| O `cppcheck --enable=unusedFunction` acha **3 das 4** | rodado sobre `src/ tools/ tests/` com `--library=qt` no código anterior à remoção: acusou `activeJobs`, `delay` e `setDelay`, e não `SendJob::program()` | Confirmação independente da análise por símbolos, mas incompleta |
| **Por que ele perdeu a quarta:** o `unusedFunction` casa a função pelo **nome simples**, sem a classe | experimento com duas iscas em `SendJob`: `program()` (nome que colide com `TailscaleClient::program()`, viva) e `programUniqueName()` (nome inédito). Só a segunda foi acusada | Ponto cego sério em código Qt, cheio de getters homônimos entre classes (`program()`, `timeout()`, `name()`). O cppcheck **não substitui** a análise por símbolos aqui; os dois se complementam |
| O `cppcheck` acusa todos os *slots* de teste como não usados | 83 avisos, todos em `tests/` | Falso positivo conhecido: quem chama os slots do `QTest` é o moc, em tempo de execução. Ruído a filtrar, não achado |
| O `clazy` (analisador específico de Qt) não acha **nada**, nos níveis 0, 1 e 2 | `clazy-standalone -p build-clang --checks=level0,level1,level2` sobre os 13 `.cpp` de `src/` | Zero avisos — e o zero é confiável porque foi provado com isca (abaixo) |
| A isca prova que as ferramentas estão de fato analisando | um laço `for (const QString name : names)` inserido de propósito em `devices.cpp` foi acusado como `-Wclazy-range-loop-reference`; a isca foi removida em seguida | Toda vez que uma ferramenta devolve "nenhum achado", vale conferir se ela está ligada. Aqui está |
| Código morto **de escopo de arquivo** o compilador já pega sozinho | a mesma isca, sendo `static`, saiu como `-Wunused-function` | Explica por que o `nm` só era necessário para função de ligação externa: o que é `static` nunca chegaria a passar despercebido |
| `misc-include-cleaner` não achou nenhum *include* sobrando | 61 avisos, **todos** do tipo "não incluído diretamente" (estilo IWYU) | Nada a remover; adotar IWYU seria mudança de estilo, não limpeza de código morto |

## 4. Arquitetura

Um artefato instalável (o plugin) sobre uma lib de núcleo testável. O envio roda
**dentro do processo do Dolphin**, de forma assíncrona.

```
[Dolphin]
   └─ tailshareitemaction.so           (plugin KIO)
        ├─ actions()      → consulta síncrona ≤300 ms, monta o submenu
        └─ SendJob (async) → ZIP (se houver pasta) → tailscale file cp
                 │                                        │
                 │                                        └── libtailshare_send
                 └─ SendNotifier → KNotification              (sobre o core)
                        └── libtailshare_notify

[tailshare-probe]  (dev, não instalado)
   └─ mesmo SendJob e mesmo SendNotifier, dirigidos pela linha de comando
```

Três bibliotecas estáticas em vez de duas, decidido ao implementar: o probe
precisa do `SendJob` sem carregar o plugin, e `KF6::Notifications` arrasta
`Qt6::Gui` (3.5). Então `tailshare_core` (regras) ← `tailshare_send` (envio,
sem UI) ← `tailshare_notify` (KNotification). O plugin e o probe linkam as três;
os testes param na segunda e rodam headless.

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

### 4.2 `tailshareitemaction.so` (o plugin) — ✅ implementado

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

Metadados JSON: `MimeTypes` com `["application/octet-stream", "inode/directory"]`
— os dois são obrigatórios (3.3) e a chave fica **dentro do objeto `KPlugin`**
(3.6). Sem `X-KDE-Show-In-Submenu`, para o item ficar no nível principal do
menu. Como o KIO reusa a instância do plugin, `actions()` não guarda estado
entre chamadas: cada menu sai de um `status` novo.

O `SendJob` disparado pelo clique é filho do `qApp`, não do plugin nem do menu:
os dois morrem segundos depois do clique e a transferência precisa sobreviver a
eles. O job se autodestrói no `finished()`.

### 4.5 `CloseGuard` — o aviso ao fechar a janela

Enquanto houver transferência viva, um filtro de eventos na janela de topo
intercepta `QEvent::Close` e pergunta *"A file is still being sent via
Tailscale"* com Continue/Cancel. Um guard por janela, morto junto com ela.

Isto **não é API suportada** — um plugin de menu de contexto não tem caminho
oficial para vetar o fechamento do host — então está isolado numa classe só e
falha aberto: se o evento não chegar, o fechamento acontece como sempre e quem
se perde é a transferência, não a janela. A solução de verdade é o helper
desanexado da v2.

### 4.3 `SendJob` — o envio, dentro do Dolphin — ✅ implementado

Disparado pelo clique, `QObject` filho do plugin, 100% assíncrono: `start()`
retorna na hora e todo o progresso sai por sinal. Estados: `Idle`,
`Compressing`, `Sending`, `Succeeded`, `Failed`, `Canceled` — `finished(bool)`
sai **uma vez só**, depois do último `stateChanged()`.

1. Se houver pasta na seleção: `Archiver` grava `KZip` em
   `$XDG_RUNTIME_DIR/tailshare-XXXXXX/<nome>.zip` **fora da thread principal**
   (`QtConcurrent::run` + `QFutureWatcher`). Falha (espaço, permissão) → estado
   `Failed` com o motivo.
2. `tailscale file cp -- <arquivos> <dnsName>:` via `QProcess` assíncrono.
3. Sucesso → `Succeeded`. Saída não-zero → `Failed` com o **stderr do tailscale**
   íntegro (é a mensagem mais útil: sem permissão, peer inválido — ver 3.5).
4. `QTemporaryDir` limpa o ZIP sempre, inclusive em erro e no cancelamento;
   verificado no teste `removesTheTemporaryArchiveAfterwards`.
5. `cancel()` levanta a flag do `Archiver` e, se já estiver enviando, manda
   `terminate()` e mata 2 s depois. O job ainda assim termina por `finished()`.

**Quem fala com o usuário é o `SendNotifier`** (4.4), não o `SendJob`: o job não
conhece KNotification nem KI18n, o que é o que permite testá-lo headless e
dirigi-lo pelo probe.

### 4.4 `SendNotifier` e as mensagens

`SendMessages` é o único lugar com as frases ("Compressing", "Sending %1 files
to %2", "Sent %1 to %2", …), o ícone de tema por estado e o mapa estado →
evento do `.notifyrc` (`sending`, `sent`, `error`). Notificação e probe dizem a
mesma coisa e há um só conjunto de strings para traduzir na Fase 4.

`SendNotifier` liga um `SendJob` a isso: uma notificação **persistente** com
ação *Cancel* acompanha a transferência e é substituída no fim por uma
efêmera de sucesso ou de erro. O progresso só aparece depois de **400 ms** de
transferência — sem isso, um envio de 20 ms deixa um popup órfão na tela por
causa do comportamento do KNotification medido em 3.5.

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
│   ├── send/                     # ✅ libtailshare_send: archiver, sendjob, sendmessages
│   ├── notify/                   # ✅ libtailshare_notify: sendnotifier
│   └── plugin/                   # ✅ tailshareitemaction, closeguard, .json
├── tools/
│   └── tailshare-probe/          # ✅ binário de dev, NÃO instalado
├── tests/                        # ✅ 9 binários QTest + CMakeLists
│   ├── fixtures/                 # ✅ 8 JSONs de status (anonimizados)
│   ├── check-translations.sh     # ✅ catálogo × código, roda no ctest
│   └── *test.cpp
├── Messages.sh                   # ✅ extração do .pot (o .pot não é versionado)
├── po/
│   └── pt_BR/tailshare.po        # ✅ catálogo, instalado por ki18n_install()
├── data/
│   └── tailshare.notifyrc        # ✅ eventos de notificação (instalado)
└── packaging/
    └── PKGBUILD                  # ✅ tailshare-git, roda a suíte no check()
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

### ✅ Fase 2 — Envio validado fora da UI (concluída, `e9a0f46`)
`Archiver`, `SendJob`, `SendMessages` (`tailshare_send`) e `SendNotifier`
(`tailshare_notify`), mais o `tools/tailshare-probe` e o `data/tailshare.notifyrc`
— este último antecipado da Fase 4 porque sem ele não havia como conferir as
notificações. **23 casos QTest novos** (`archivertest`, `sendjobtest`), 83 no
total em 7 binários, `ctest` 100%.

**Verificado contra a tailnet real** (peer `home-tuzin`, tailscale v1.102.2),
depois de `sudo tailscale set --operator=$USER` (3.5):

| Caso | Resultado |
|---|---|
| dois arquivos avulsos, um chamado `-v` e outro com espaço | `Sent 2 files`, 711 ms, sem ZIP |
| uma pasta com subpasta, acento e 200 KB binários | `pasta de teste.zip`, 976 ms |
| seleção mista (pasta + dois arquivos) | `probe.zip` com os três, 691 ms |
| notificações | medidas no barramento DBus: progresso só acima de 400 ms, `CloseNotification` antes da final, e os eventos `sending`/`sent`/`error` do `.notifyrc` resolvem |

Os caminhos lentos foram exercitados com um tailscale falso (`--program`):
cancelamento no meio do envio termina em `Canceled` com `terminate()`, e
`--timeout 1000` sobre um envio de 3 s falha aos ~950 ms. O que **não** foi
verificado: o conteúdo dos arquivos no dispositivo que recebeu — a evidência
aqui é o `exit 0` do `tailscale file cp`, que significa peer aceitou.

### ✅ Fase 3 — Plugin do menu (concluída, `b9227bd` + `72cf376`)
Entregue: `tailshareitemaction` (metadados, regras de ocultação, ícones por SO,
itens desabilitados com tooltip, ligação com o `SendJob`) e o `CloseGuard` do
spike (4.5). **18 casos novos no `plugintest`**, que carrega o `.so` pelo mesmo
caminho do KIO e dirige a tailnet por `$TAILSHARE_TAILSCALE`: os cinco casos de
MIME, menu montado a partir de `running-tailnet.json`, item desabilitado com
motivo, e o submenu sumindo com backend parado, sem login, tailnet vazia, JSON
malformado, comando falhando, comando pendurado (`sleep 5`, volta em <1 s) e
seleção remota `sftp://`.

**QA manual no Dolphin real** (não automatizável aqui: sessão Wayland, sem
`xdotool`/`ydotool`), feito pelo usuário nesta máquina:

| Caso | Observado |
|---|---|
| Submenu no menu de contexto | aparece, no nível principal |
| Envio ponta a ponta pelo menu, para o `home-tuzin` | funcionou |
| `tailscale down` | o submenu **some**, sem travar o menu |
| Ação *Cancel* da notificação persistente | cancela a transferência |
| Fechar a janela durante um envio (spike de 4.5) | **o aviso aparece** — o event filter em `QEvent::Close` funciona no Dolphin 26.04 |

O aviso de fechamento continua sendo API não suportada: funciona nesta versão,
pode quebrar em outra, e o plano B (aceitar o cancelamento e notificar) segue
valendo. A prova dos cinco formatos de seleção é a do `plugintest`; o que se
observou no Dolphin foi o submenu aparecendo em uso normal.

### ✅ Fase 4 — i18n e empacotamento (concluída, `e518a3c` + `af175e4`)
Entregue: `Messages.sh` (extração com o conjunto de keywords do KDE, 22
mensagens), `po/pt_BR/tailshare.po` completo, `ki18n_install(po)` no CMake com o
espelho para `build/share/locale` (3.7), `Name[pt_BR]`/`Description[pt_BR]` no
JSON do plugin, `Name[pt_BR]`/`Comment[pt_BR]` no `.notifyrc`,
`packaging/PKGBUILD` (`tailshare-git`, com `check()` rodando a suíte) e a seção
*Translations* do README. As instruções de instalação, desinstalação e do grupo
`[Show]` do `kservicemenurc` já estavam no README desde a Fase 3; ganharam o
caminho do `makepkg`.

**Dois testes novos**, ambos no `ctest`: `i18ntest` (9 casos) compara as strings
que o usuário lê — motivos de recusa, título e texto das notificações de um
`SendJob` real, nas duas formas de plural — contra o catálogo que o build
acabou de compilar; `translationstest` roda o `Messages.sh` num diretório
temporário e reprova, via `msgcmp` e `msgfmt --check`, qualquer catálogo de
`po/` que tenha ficado para trás do código.

**Verificado por teste automatizado:** `ctest` 100% (11 testes) com
`LANG=en_US` **e** com `LANG=pt_BR`.

**QA manual sobre o pacote instalado** (o `sudo` desta máquina pede senha, então
o `makepkg -si` foi rodado pelo usuário; o resto foi observado nesta sessão):

| Caso | Observado |
|---|---|
| `makepkg -si` a partir do `packaging/` | clona por SSH, compila, roda a suíte no `check()` e instala `tailshare-git 0.1.0.r14.af175e4-1` |
| Conteúdo do pacote (`pacman -Ql`) | exatamente quatro arquivos: o `.so` em `/usr/lib/qt6/plugins/kf6/kfileitemaction/`, o `tailshare.mo` em `/usr/share/locale/pt_BR/LC_MESSAGES/`, o `.notifyrc` em `/usr/share/knotifications6/` e a licença |
| Dolphin reiniciado **sem** variável de ambiente nenhuma | o submenu aparece, vindo de `/usr`; envio e notificações funcionam |
| Catálogo instalado é encontrado sem apontar para o build tree | `env -u XDG_DATA_DIRS LANGUAGE=pt_BR ./build/bin/tailshare-probe --list` devolve *"Este dispositivo está offline."*, e `LANGUAGE=en` devolve o original |
| `LANGUAGE=pt_BR dolphin` | o item do menu lê **Compartilhar pelo Tailscale** e o tooltip do dispositivo cinza lê **Este dispositivo está offline.** |
| Nome e descrição em *Configurar o Dolphin → Menu de contexto* | em pt-BR, vindos do JSON (caminho de tradução diferente do catálogo — ver 3.7) |

Nenhum passo dependeu de `kbuildsycoca6`, como 3.3 previa.

### ✅ Fase 5 — QA manual (concluída, `1ee48e1` … `da4e024`)
Os onze casos do roteiro foram percorridos no Dolphin com o pacote instalado, e
estão na tabela *What has been checked by hand* do README: arquivo único, vários
arquivos, nome com espaço e acento, pasta, pasta de 400 MB (a janela continua
respondendo enquanto o ZIP é escrito), seleção mista, dispositivo cinza com
motivo no tooltip, cancelamento pela notificação, aviso ao fechar a janela
durante a transferência, e seleção não-local (via `zip:`) sem submenu nenhum.

O caso da queda de rede no meio do envio rendeu o achado de 3.8 e mudou a tabela
de riscos da seção 8. A fixture `stopped.json` foi refeita a partir de um
`tailscale down` real (3.8).

Dois casos **não** puderam ser observados nesta tailnet, por falta de
dispositivo com Taildrop desabilitado e de uma tailnet só com o self: seguem
cobertos apenas por teste automatizado, e o README diz isso em vez de deixar
parecer verificado.

A fixture `needs-login.json` foi refeita a partir de um `tailscale logout` real
e também estava errada (3.8) — duas de duas. E o estado `NeedsLogin`, que nunca
tinha sido visto no Dolphin real (a Fase 3 observou `Stopped`), foi conferido
com a sessão deslogada: **o item não aparece no menu**, nem cinza.

**Pronto:** os onze casos do roteiro têm comportamento observado e documentado
na tabela do README, com as duas exceções nomeadas ali como cobertas apenas por
teste.

Roteiro fechado: arquivo único, múltiplos arquivos, pasta, pasta grande, nome com
espaços e acentos, peer offline, peer sem Taildrop, tailnet só com self,
`tailscale down` no meio do envio, fechar o Dolphin durante o envio, seleção em
`sftp://`. Inclui conferir a saída real de `status --json` com `tailscale down`
e sem login, para corrigir as fixtures `stopped.json` e `needs-login.json` (3.4).
**Pronto quando:** cada caso tem comportamento observado e documentado no README.

### Fase 6 — Verificações antes da publicação (em andamento)
Pedida antes de tornar o repositório público: varredura de código morto e de
vulnerabilidades. **Código morto: feito** — quatro funções removidas, achadas
por análise de símbolos e confirmadas em parte pelo `cppcheck`; `clang-tidy`,
`clazy` (níveis 0 a 2) e os dois compiladores rodam sem um aviso, e o "nenhum
achado" de cada ferramenta foi provado com isca (3.9). **Falta** a verificação
de vulnerabilidades.
**Pronto quando:** as ferramentas rodam limpas ou cada aviso restante tem uma
linha dizendo por que fica.

---

## 7. Dependências

**Build:** `extra-cmake-modules` (instalado), `cmake`, `g++`, Qt 6.11 —
`Qt6::Core`, `Qt6::Concurrent` (compactar fora da thread principal) e
`Qt6::Gui` (arrastado pelo KNotifications, ver 3.5); mais `Qt6::Test` para a
suíte (`BUILD_TESTING`, ligado por padrão via ECM).
**KF6 (já presentes):** KIO 6.28, KCoreAddons, KI18n, KNotifications, KArchive, KWidgetsAddons.
**Runtime:** `tailscale` ≥ 1.102 no PATH; Plasma 6, Dolphin 26.04 — e o usuário
precisa ser *operator* do tailscaled (`sudo tailscale set --operator=$USER`),
senão todo envio é recusado com `Access denied` (3.5).

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
| Envio a peer offline fica pendurado (a CLI tenta assim mesmo, `file.go:235`) | Itens offline já vêm desabilitados; o timeout do `SendJob` existe mas fica **desligado** por padrão (ver 3.5) — a saída é a ação *Cancel* da notificação. A Fase 5 deu a essa decisão uma justificativa que ela ainda não tinha: um envio interrompido por queda de rede **retoma e conclui** (3.8), então um timeout mataria transferência que teria dado certo |
| Usuário não é *operator* do tailscaled e todo envio falha | A mensagem do tailscale é repassada íntegra e já ensina o comando; documentado como pré-requisito no README |
| Plugin não aparece por MIME não casado | Casos de MIME viram checklist explícito da Fase 3 |
| Plugin novo não é detectado | Documentar reinício do Dolphin no README (sycoca não se aplica) |
| Usuário desabilita o plugin sem querer | Documentar o grupo `[Show]` do `kservicemenurc` no README |

---

## 9. Pontos ainda em aberto

As seis perguntas da versão anterior foram todas respondidas e viraram linhas da
seção 1. Com as duas fixtures conferidas na Fase 5, resta um ponto — e ele não
tem código atrás:

1. **Estado com login pendente** — o `AuthURL` foi observado **vazio** logo após
   o `logout` (3.8). Falta ver o estado intermediário, com um `tailscale up`
   esperando autenticação no navegador, onde a URL provavelmente aparece.
   Nenhum caminho de código depende do `AuthURL`, então isto é curiosidade
   documentada, não risco.

Resolvidos desde a versão anterior:

- ~~**Aviso ao fechar o Dolphin**~~ — o `CloseGuard` funciona no Dolphin real
  (Fase 3). Segue fora da API suportada, com plano B mantido.
- ~~**Seleção mista arquivo + pasta**~~ — casa; ver 3.6.
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

**KDE Frameworks 6 — Fase 2** (6.28 instalado):
- `/usr/include/KF6/KNotifications/knotification.h` — `KNotificationAction`
  (l. 30), sobrecargas de `event()` (l. 798-803), `Persistent` (l. 174),
  `update()` privado (l. 649)
- `/usr/lib/cmake/KF6Notifications/KF6NotificationsTargets.cmake:72` — dependência de `Qt6::Gui`
- `/usr/include/KF6/KArchive/karchive.h` — `addLocalFile` (l. 138),
  `addLocalDirectory` (l. 153), `errorString()` (l. 91)
- `dbus-monitor --session` em `org.freedesktop.Notifications` durante três
  execuções do `tailshare-probe --notify` (job rápido, job de 3 s, cancelamento)
- `/usr/share/ECM/kde-modules/KDEInstallDirs6.cmake:291` — `KNOTIFYRCDIR`

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

**Empacotamento — Fase 4** (conferido depois de o pacote já estar instalado, a
pedido do usuário, contra a documentação oficial e não contra a memória):
- `man 5 PKGBUILD`, do `pacman 7.1.0` instalado nesta máquina — campos
  obrigatórios (`pkgname`, `pkgver`, `pkgrel`, `arch`), `pkgver()` recalculado
  após a extração, `package()` rodando sob `fakeroot` com `$pkgdir` como raiz do
  pacote, `check()` entre `build()` e `package()`, e a forma
  `source=('directory::url#fragment')` com prefixo `vcs+` para fontes VCS
- Wiki do Arch, via Context7 — identificadores **SPDX** no campo `license`;
  `base-devel` presumido instalado e seus membros fora do `makedepends`; sufixo
  `-git` no `pkgname` de pacote VCS, com `provides`/`conflicts` explícitos e a
  ferramenta de VCS no `makedepends`
- `pacman -Si base-devel` nesta máquina — `gettext` está lá, `git` e `cmake` não
