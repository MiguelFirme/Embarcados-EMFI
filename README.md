# Plataforma didática de Fault Injection (RP2040)

Projeto de laboratório com dois Raspberry Pi Pico e uma CLI Python. O controlador recebe comandos por USB CDC, aguarda o trigger do Pico alvo, espera um atraso configurado, gera um pulso lógico em `EMFI_TRIGGER_OUT` e lê a resposta do alvo por UART. Uma varredura repete isso para vários atrasos e salva CSV.

**`EMFI_TRIGGER_OUT` é apenas um GPIO de 3,3 V.** `SIMULATION_MODE=1` é a única configuração suportada. Não existe estágio EMFI de alta tensão, controle de energia ou interface elétrica para um driver externo. A abstração `emfi_arm()`, `emfi_fire()` e `emfi_disarm()` separa esta implementação lógica de uma eventual interface futura.

## Arquitetura

```text
PC / pc/main.py
    │ USB serial, protocolo ASCII
    ▼
Pico controlador ── GP15 EMFI_TRIGGER_OUT ──> instrumento de medição
    │                   (pulso lógico)
    ├── GP14 TARGET_TRIGGER_IN <────────────── GP14 Pico alvo
    ├── GP16 TARGET_RESET_OUT ────────────────> GP16 Pico alvo (soft reset)
    ├── GP4  UART1 TX ────────────────────────> GP5  UART1 RX
    └── GP5  UART1 RX <──────────────────────── GP4  UART1 TX
                  GND <───────────────────────> GND
```

O controlador é dividido em `main.c` (entrada USB), `protocol.c` (texto), `experiment.c` (estados e sweep), `target.c` (UART/reset) e `emfi.c` (GPIO de pulso). O segundo Pico executa `target_firmware/main.c`. `pc/serial_interface.py` isola o transporte, `pc/experiment.py` interpreta resultados e `pc/logger.py` grava CSV.

### Pinout inicial do Pico controlador

| Sinal | GPIO | Direção | Observação |
|---|---:|---|---|
| `EMFI_TRIGGER_OUT` | 15 | saída | Pulso alto de lógica 3,3 V; somente medição nesta versão |
| `TARGET_TRIGGER_IN` | 14 | entrada | Borda de subida, pull-down interno |
| `TARGET_RESET_OUT` | 16 | saída | Pedido de reset **por software** ativo baixo ao firmware de teste |
| `TARGET_UART_TX` | 4 | saída | UART1, 115200, 8N1 |
| `TARGET_UART_RX` | 5 | entrada | UART1, 115200, 8N1 |
| `STATUS_LED` | 25 | reservado | Não utilizado; referência ao Pico RP2040 padrão |

O GP16 do controlador vai apenas ao GP16 do firmware alvo fornecido. **Não o ligue diretamente ao pino RUN ou à alimentação do Pico**. Mantenha terra comum entre os Picos. A saída GP15 não deve ser ligada diretamente a circuitos de alta tensão. Teste primeiro com analisador lógico ou osciloscópio.

## Requisitos, compilação e gravação

Requer [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), CMake, Ninja ou outro gerador, `arm-none-eabi-gcc` e Python 3. Em PowerShell:

```powershell
git clone --recursive https://github.com/raspberrypi/pico-sdk.git C:\pico\pico-sdk
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'
cmake -S . -B build -G Ninja -DPICO_BOARD=pico
cmake --build build
python -m pip install -r pc/requirements.txt
```

O build deve gerar `build/enfi_controller.uf2` e `build/enfi_target.uf2`. Grave cada arquivo no Pico correspondente: conecte segurando **BOOTSEL**, solte e copie o `.uf2` para a unidade `RPI-RP2`. O firmware controlador usa `stdio` por USB; o firmware alvo usa apenas UART1. Veja também o [guia oficial do SDK](https://github.com/raspberrypi/pico-sdk#quick-start-your-own-project).

## Protocolo serial

Linhas ASCII maiúsculas terminadas em `\n` ou `\r`. Valores de delay e largura são **microssegundos inteiros** (`_US`); timeouts são milissegundos (`_MS`). Não há comando em nanossegundos, porque esta implementação por software não oferece essa resolução.

| Comando | Efeito |
|---|---|
| `PING`, `STATUS` | Conexão e estado/configuração |
| `ARM`, `PULSE`, `DISARM` | Pulso manual; `PULSE` exige armamento e o consome |
| `SET WIDTH_US 100` | Largura de 1 a 1.000.000 µs |
| `SET DELAY_US 500` | Atraso de 0 a 1.000.000 µs |
| `SET TRIGGER_TIMEOUT_MS 500` | Timeout de trigger de 1 a 60.000 ms |
| `SET RESULT_TIMEOUT_MS 500` | Timeout da resposta de 1 a 60.000 ms |
| `SET RESET_TARGET 0` ou `1` | Reset por software antes de cada tentativa |
| `RESET TARGET` | Pedido de reset por software imediato |
| `RUN` | Uma tentativa acionada pelo target |
| `SWEEP START 0 5000 50 5` | Atrasos 0, 50, …, 5000 µs; 5 repetições por ponto |

`RUN` e `SWEEP START` respondem `OK` imediatamente. Durante a execução, o Pico envia `EVENT TRIGGER`, `EVENT PULSE` e:

```text
RESULT 00001 50 100 OK 10423 RESULT:02FB0408
```

Os campos são `experiment_id delay_us pulse_width_us classe elapsed_us resposta_target`. O tempo é desde o envio de `RUN` ao alvo até o resultado; não é o tempo de pulso. Timeout usa resposta `-`. Ao terminar uma varredura, envia `SWEEP COMPLETE`. Erros têm formato `ERROR <CÓDIGO>`. `DISARM` interrompe uma execução; nesse caso não há resultado para a tentativa interrompida. Limites: 95 caracteres por linha e 10.000 tentativas por sweep.

O alvo espera `RUN\n` em UART1, levanta GP14, soma os inteiros de 1 a 10.000, mantém uma janela de 10 ms, abaixa GP14 e envia `RESULT:02FB0408`. O controlador classifica a resposta exata como `OK`, outro `RESULT:` como `FAULT`, `RESET` como `RESET`, ausência de resposta como `TIMEOUT` e outras respostas como `UNKNOWN`. A classificação está concentrada em `classify()` para futura substituição. O resultado `OK` apenas confirma a resposta digital esperada; não prova que o pulso afetou fisicamente o alvo.

Se a resposta chegar **antes** do instante planejado para o pulso, a tentativa termina como `UNKNOWN` e o pulso é cancelado, pois a janela de teste já terminou.

## CLI e CSV

```powershell
python pc/main.py ports
python pc/main.py --port COM5 ping
python pc/main.py --port COM5 status
python pc/main.py --port COM5 set-width 100
python pc/main.py --port COM5 arm
python pc/main.py --port COM5 pulse
python pc/main.py --port COM5 run --delay 50 --width 100 --output results.csv
python pc/main.py --port COM5 sweep --start 0 --end 5000 --step 50 --repetitions 5 --width 100 --output results.csv
```

Se houver exatamente uma porta serial no computador, `--port` pode ser omitido. Cada resultado é gravado imediatamente para preservar progresso mesmo se a varredura for interrompida. O CSV contém `timestamp,experiment_id,delay_ns,pulse_width_ns,result,target_response,elapsed_us`. Os valores em nanossegundos do CSV são **conversões de microssegundos**, não medições com precisão de nanossegundos. O timestamp UTC é obtido no PC ao receber a linha.

## Estados e limites temporais

O fluxo é `CONFIGURED → WAIT_TRIGGER → DELAY → PULSE → WAIT_RESULT → COMPLETE`, com `IDLE`, `ARMED` e `ERROR` reservados para controle e diagnóstico. Uma interrupção GPIO registra o instante da borda de subida; o laço principal consulta o relógio em microssegundos e chama `emfi_fire()`. Durante o pulso, `sleep_us()` mantém o GPIO alto. Há latência e jitter do laço, da interrupção, das chamadas SDK e da saída USB; portanto **delay e largura reais devem ser medidos no hardware**. O código não promete borda com resolução ou exatidão de nanossegundos. Para temporização mais fina, a próxima revisão deve testar PIO ou periféricos temporizados e calibrar com instrumento.

O projeto foi **compilado** neste ambiente com Pico SDK 2.3.0, CMake 4.3.4, GCC ARM 15.2.1 e Python 3.11; os dois arquivos `.uf2` foram gerados. A sintaxe da CLI e um teste do parser/CSV também passaram. **Ainda não houve teste nos Picos nem medição das bordas**: os primeiros testes em bancada devem confirmar pinout, UART, timeouts e limites elétricos antes de conectar qualquer estágio externo.

## Roadmap

- Medir latência/jitter e, se necessário, migrar trigger/pulso para PIO ou timer de hardware.
- Adicionar testes automatizados de protocolo, análise de falhas e gráfico de taxa de falhas por delay.
- Evoluir para outros targets, trigger externo, diferentes padrões de trigger, SWD e analisador lógico.
- Considerar GUI, gráficos em tempo real e banco de resultados.
- Definir separadamente a interface elétrica de eventual módulo EMFI externo ou voltage glitching.
