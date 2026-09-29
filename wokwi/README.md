# Simulação Wokwi do controlador EMFI

Esta simulação executa o **firmware C do Pico controlador** e um **modelo C do Target** ligado a ele por GPIO e UART. O modelo é compilado para WebAssembly e representa as interfaces externas do segundo Pico. O [FAQ oficial do Wokwi](https://docs.wokwi.com/faq#how-do-i-use-multiple-microcontrollers-in-a-single-project) informa que dois microcontroladores ligados por fios no mesmo projeto ainda não são suportados. O firmware independente do segundo Pico continua em `target_firmware/main.c` e é compilado para uso nos dois Picos físicos; ele não é executado pela simulação Wokwi.

As mudanças de resultado no Wokwi são **falhas artificiais por entrada digital**. Elas não medem probabilidade, energia ou eficácia de EMFI físico. GP15 continua sendo somente um pulso lógico de 3,3 V. A entrada `FIRE` do modelo existe exclusivamente na simulação; a ligação GP15 → `FIRE` **não deve ser reproduzida como ligação de injeção física**.

## Arquivos e construção

| Arquivo | Papel |
|---|---|
| `../wokwi.toml` | Firmware, chip WASM, captura VCD e porta RFC2217 |
| `../diagram.json` | Pico, modelo de Target, fios, botão de timeout e analisador lógico |
| `chips/target-model.chip.c` | Máquina de estados do Target simulado |
| `chips/target-model.chip.json` | Pinos do modelo |
| `chips/target-model.chip.wasm` | Modelo compilado, carregado pelo Wokwi |
| `validation.scenario.yaml` | Sequência de testes A, B, D, E, F e G para Wokwi CLI |

Os arquivos `wokwi.toml` e `diagram.json` ficam na **raiz do repositório** porque a [extensão procura a configuração no diretório do projeto](https://docs.wokwi.com/vscode/project-config). Os arquivos específicos do Target simulado ficam nesta pasta.

No terminal da raiz, com Pico SDK, CMake, Ninja, compilador ARM e Python 3 disponíveis:

```powershell
$env:PICO_SDK_PATH = 'C:\Users\migue\.pico-sdk\sdk\2.3.0'
cmake -S . -B build -G Ninja -DPICO_BOARD=pico -Dpicotool_DIR='C:/Users/migue/.pico-sdk/picotool/2.3.0/picotool'
cmake --build build
python -m pip install -r pc/requirements.txt
```

Se `cmake`, `ninja` ou o compilador não estiverem no `PATH`, use o terminal configurado pela extensão Raspberry Pi Pico ou adicione os diretórios correspondentes de `C:\Users\migue\.pico-sdk`. Nesta máquina, o build usou Python 3.11, SDK 2.3.0, CMake 4.3.4 e GCC ARM 15.2.1. Ajuste os caminhos se a instalação for diferente. O build gera `build/enfi_controller.uf2` e `build/enfi_target.uf2`; **somente o controlador** é carregado pelo Wokwi.

O modelo `chips/target-model.chip.wasm` já foi compilado. Após editar `chips/target-model.chip.c`, recompile com [Wokwi CLI 0.20 ou mais recente](https://docs.wokwi.com/guides/custom-chips-to-wasm):

```powershell
wokwi-cli chip compile wokwi/chips/target-model.chip.c -o wokwi/chips/target-model.chip.wasm
wokwi-cli lint . --warnings-as-errors
```

## Pinos e sinais

| Pico controlador no Wokwi | Modelo Target | Função |
|---|---|---|
| GP4, UART1 TX | RX | Envia `RUN\n` |
| GP5, UART1 RX | TX | Recebe `RESULT:...` |
| GP14, entrada | TRIGGER, saída | Detecta início da tentativa |
| GP15, saída | FIRE, entrada | **Somente** perturbação digital artificial no Wokwi |
| GP16, saída | RESET, entrada | Pedido de reset lógico ativo baixo |
| 3V3 | VCC | Alimentação lógica simulada |
| GND | GND | Referência comum |

O modelo expõe `EXEC` para mostrar a janela de execução e `HOLD` para suprimir uma resposta. `HOLD` está ligado a um botão; segurá-lo ao enviar `RUN` produz um **timeout de resultado**. No segundo Pico físico, GP14 é trigger, GP4/GP5 são UART, GP16 é reset lógico e GP18 marca execução. O pino `FIRE` artificial **não integra o pinout físico**.

### Logic Analyzer

| Canal | Sinal | O que observar |
|---|---|---|
| D0 | `TARGET_TRIGGER_OUT` | Borda de início e duração da tentativa |
| D1 | `EMFI_FIRE_OUT` | Borda do pulso após o atraso configurado |
| D2 | `TARGET_EXECUTION` | Janela artificial de execução do Target |
| D3 | `TARGET_RESET` | Pedido de reset ativo baixo |

O modelo levanta `TRIGGER` no início, levanta `EXEC` **2.000 µs** depois, mantém `EXEC` alto por **5.000 µs**, abaixa `TRIGGER` após **10.000 µs** e envia o resultado pela UART. Se a borda de subida de `FIRE` chegar enquanto `EXEC` estiver alto, retorna `RESULT:DEADBEEF`; caso contrário, retorna `RESULT:02FB0408`. A rotina física usa a mesma soma determinística, mas não recebe `FIRE` no modo normal.

## Iniciar no VS Code e testar

1. Abra **a raiz deste repositório** no VS Code. Instale/ative a extensão [Wokwi for VS Code](https://docs.wokwi.com/vscode/getting-started) e sua licença, caso ela solicite.
2. Compile o projeto e confirme a existência de `build/enfi_controller.uf2` e `wokwi/chips/target-model.chip.wasm`.
3. Pressione `F1` e escolha **Wokwi: Start Simulator**. Identifique o único Pico desenhado como **controller** e o chip verde como **Target model**. O modelo substitui o segundo Pico devido à limitação do simulador.
4. Abra o **Serial Monitor** do Wokwi. O `diagram.json` o configura para aparecer desde o início. Digite `PING` e pressione Enter: a resposta esperada é `OK PONG`.
5. Para um teste de falha artificial, envie `SET WIDTH_US 100`, `SET DELAY_US 3000` e `RUN`. Observe `EVENT TRIGGER`, `EVENT PULSE` e um `RESULT ... FAULT ... RESULT:DEADBEEF`.
6. Para um teste normal, envie `SET DELAY_US 0` e `RUN`. Espere `RESULT ... OK ... RESULT:02FB0408`.
7. Para timeout, envie `SET RESULT_TIMEOUT_MS 50`, segure o botão **Hold for TIMEOUT**, envie `RUN` e espere `RESULT ... TIMEOUT`. Solte o botão antes do próximo teste.
8. Para sweep, envie `SWEEP START 0 5000 1000 5`; espere as linhas `RESULT` e `SWEEP COMPLETE`. Cada ponto é medido 5 vezes. O comando usa **microssegundos**, não nanossegundos.
9. Pare a simulação. O analisador salva `wokwi/logic-capture.vcd`. Abra esse arquivo com um visualizador VCD, como Surfer, PulseView ou GTKWave. Compare D0 (trigger), D1 (FIRE) e D2 (execução). A [documentação do Logic Analyzer](https://docs.wokwi.com/guides/logic-analyzer) explica a captura VCD.

O código registra o instante de `TRIGGER` por interrupção, mas gera `FIRE` no laço principal. Compare as bordas no VCD para descobrir o atraso **observado** e seu jitter. Os comandos e o CSV usam µs; valores em `delay_ns` e `pulse_width_ns` no CSV são conversões por 1.000, sem precisão adicional.

## CLI Python com a simulação

O [Wokwi documenta](https://docs.wokwi.com/vscode/project-config#serial-port-forwarding) a porta RFC2217 para acessar a serial simulada por PySerial. `wokwi.toml` usa a porta TCP local 4000. Com a simulação aberta e a aba visível no VS Code:

```powershell
python pc/main.py --port rfc2217://localhost:4000 ping
python pc/main.py --port rfc2217://localhost:4000 run --delay 3000 --width 100 --output wokwi/results.csv
python pc/main.py --port rfc2217://localhost:4000 sweep --start 0 --end 5000 --step 1000 --repetitions 10 --width 100 --output wokwi/results.csv
python pc/main.py plot --input wokwi/results.csv --output wokwi/fault-rate.png
```

Use o Serial Monitor **ou** a CLI para digitar comandos durante cada tentativa, evitando respostas intercaladas. `plot` agrupa por delay solicitado e calcula `FAULT / total de resultados` em cada ponto; é uma frequência de **falhas artificiais no modelo**.

## Validação e limites

O firmware dos dois Picos compilou, o chip WASM compilou e o diagrama passou no `wokwi-cli lint --warnings-as-errors`. A CLI Python passou na verificação de sintaxe e o parser/CSV em teste local. **A simulação Wokwi ainda não foi executada nesta sessão**; por isso, as respostas acima são expectativas do código, não resultados observados. O arquivo `validation.scenario.yaml` prepara os testes A–G para execução automatizada com Wokwi CLI, que exige [token próprio](https://docs.wokwi.com/wokwi-ci/cli-usage). O teste C requer conferir a distância entre bordas no VCD.

O modelo substitui apenas as interfaces externas do segundo Pico e não executa instruções do seu firmware RP2040. Isso preserva a separação lógica, mas diferenças de temporização do segundo firmware só podem ser descobertas com testes nos Picos físicos ou em um simulador que suporte dois RP2040 conectados.
