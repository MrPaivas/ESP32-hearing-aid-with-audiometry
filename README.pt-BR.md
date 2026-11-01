# ESP32 Hearing Aid with Audiometry

[![CI](https://github.com/MrPaivas/ESP32-hearing-aid-with-audiometry/actions/workflows/ci.yml/badge.svg)](https://github.com/MrPaivas/ESP32-hearing-aid-with-audiometry/actions/workflows/ci.yml)
[![Licença: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

[English](README.md)

Aparelho auditivo experimental com ESP32-WROOM-32, microfone I2S INMP441 e DAC I2S PCM5102A (saída P2), com audiometria tonal embutida.

> [!WARNING]
> **Este não é um dispositivo médico.** Ele toca som amplificado diretamente nos ouvidos e pode causar dano auditivo permanente. Leia o [SAFETY.pt-BR.md](SAFETY.pt-BR.md) antes de montar ou usar.

## Por que este projeto existe

O objetivo é explorar até onde peças de baixo custo chegam como aparelho auditivo: o dispositivo mede quais frequências você tem dificuldade de ouvir em cada ouvido e amplifica o som ambiente de acordo, com o menor atraso possível.

## Status e limitações

Este é um protótipo em protoboard.

- A latência de 4–5 ms é uma estimativa, não uma medição.
- Não há validação clínica.
- O monitor de bateria vem desligado (`BATT_MONITOR_ENABLED` é 0 em `src/board.h`).
- Ainda não há PCB publicada.
- A senha padrão do Wi-Fi é pública; veja o [SECURITY.md](SECURITY.md).

## Funcionalidades

- **Audiometria tonal** por ouvido: Hughson-Westlake modificado (desce 10 / sobe 5 dB, limiar = 2 respostas ascendentes), 250 Hz–8 kHz, reteste de 1 kHz, intervalo aleatório entre tons e contagem de falsos positivos. Tons pulsados com rampas de 20 ms.
- **Aparelho auditivo**: prescrição **NAL-R** por ouvido, equalizador de 8 bandas de fase mínima (biquads), compressão WDRC, limitador de saída (MPO) e volume no encoder.
- **Baixa latência**: 32 kHz, blocos de 1 ms, I2S full-duplex.
- **Página web** (modo configuração, Wi-Fi AP): audiograma, curva de ganho, ajuste fino por banda, calibração e tons de teste.

```mermaid
flowchart LR
    M[Microfone INMP441] --> H[Passa-altas 100 Hz]
    H --> W[WDRC]
    W --> G[Volume e ganho por ouvido]
    G --> E[EQ de 8 bandas por ouvido]
    E --> L[Limitador MPO]
    L --> D[DAC PCM5102A]
    D --> P[Fones]
```

## Lista de materiais

| Peça | Observações |
|---|---|
| ESP32-WROOM-32 DevKit | O LED azul da placa (D2) é a luz de status |
| Módulo INMP441 | Microfone I2S |
| Módulo PCM5102A | DAC I2S com saída P2 |
| Encoder rotativo com botão | Qualquer encoder mecânico deve servir |
| Célula 18650 e carregador TP4056 com proteção | Para uso com bateria |
| Regulador LDO de 3,3 V | Para uso com bateria |
| Fones fechados | Fones abertos causam microfonia |

Veja [docs/pt-BR/hardware.md](docs/pt-BR/hardware.md) para ligações, alimentação e calibração.

## Início rápido

1. Faça as ligações seguindo [docs/pt-BR/hardware.md](docs/pt-BR/hardware.md).
2. Instale o [PlatformIO](https://platformio.org/) (extensão do VS Code ou `pip install platformio`).
3. Compile e grave:

```sh
pio run -t upload
```

4. Siga [Primeiro uso](#primeiro-uso), abaixo.

## Compilar, testar e gravar

```sh
pio run                       # compila
pio run -t upload             # grava
pio device monitor            # log serial (115200)
pio test -e native            # testes no PC (precisa de gcc no PATH)
```

Se a gravação parar em `Connecting...` ou falhar com `Invalid head of packet`, coloque a placa em modo de gravação na mão: **segure BOOT, aperte e solte EN, e solte BOOT** depois de 2–3 s. Não mexa na placa até terminar.

O log serial mostra a cada 10 s o tempo de DSP por bloco (deve ficar bem abaixo de 1000 µs) e o nível de cada slot do microfone.

## Como usar

Todo o controle é feito pelo **encoder** (girar + apertar) e a resposta vem pelo **LED azul da própria placa (D2)**.

### Primeiro uso

1. Conecte os fones (de preferência fechados) e coloque o **volume no mínimo**.
2. Ligue o aparelho. Sem audiograma salvo, a **audiometria começa sozinha** 3 s depois (veja abaixo).
3. Ao terminar, o aparelho já passa a funcionar com o ganho prescrito para os seus limiares.

### Controles

| Ação | Função | LED |
|---|---|---|
| Girar | Volume (±2 dB por clique) | Pisca rápido ao chegar no limite |
| Clique | Mudo liga/desliga | Mudo: 2 piscadas curtas · Som ligado: aceso 0,4 s |
| Duplo clique | Nível da bateria | 1 a 4 piscadas (4 = cheia) |
| Segurar 3 s | Inicia a audiometria (durante o teste: **cancela**) | Veja abaixo |
| Segurar ao ligar | Modo configuração (Wi-Fi + página web) | Aceso enquanto segura, depois 3 piscadas |

Com o divisor da bateria montado e `BATT_MONITOR_ENABLED` em 1 (`src/board.h`), abaixo de 3,40 V o LED pisca 3 vezes a cada minuto e abaixo de 3,20 V o aparelho desliga sozinho para proteger a bateria. Sem o divisor, o nível do duplo clique não é confiável.

### Audiometria

Faça em um lugar **silencioso**, com os fones bem colocados. Leva de 10 a 15 minutos.

1. **Segure o botão por 3 s.** O LED pisca 2 vezes e fica aceso durante todo o teste.
2. No início de cada ouvido o LED dá piscadas lentas: **1 = ouvido direito** (testado primeiro), **2 = ouvido esquerdo**.
3. Você ouvirá bipes curtos (3 pulsos) em intervalos aleatórios. **Aperte o botão sempre que ouvir**, mesmo que bem fraquinho. O LED apaga rapidamente para confirmar o aperto.
4. Não aperte sem ouvir: apertos no silêncio contam como falso positivo e atrasam o próximo tom.
5. Ao final:
   - **3 piscadas rápidas** = teste concluído e salvo; o aparelho volta ao modo aparelho auditivo.
   - **1 piscada longa** = teste cancelado; nada foi salvo e o audiograma anterior continua valendo.

Para cancelar no meio, segure o botão por 3 s. Se ninguém responder a nenhum tom, o resultado é descartado.

### Modo configuração (página web)

1. Desligue o aparelho, **segure o botão e ligue**. Solte quando o LED piscar 3 vezes.
2. No celular ou computador, conecte na rede **`HearingAid-XXXX`** (senha `aparelho123`).
3. Abra **http://192.168.4.1**.

Na página dá para ver o audiograma e a curva de ganho, fazer ajuste fino por banda, escolher o tipo de fone, calibrar e iniciar a audiometria (inclusive a **calibração biológica**). Veja [Calibração](docs/pt-BR/hardware.md#calibração).

## Estrutura do projeto

```
lib/hacore/     lógica pura (sem ESP-IDF), testável no PC
  audiometry.*    procedimento Hughson-Westlake
  nalr.*          prescrição NAL-R
  eq_fit.*        EQ 8 bandas + ajuste do alvo
  dynamics.*      WDRC + limitador
  ha_dsp.*        cadeia completa do aparelho
  fitting.*       audiograma + configurações -> parâmetros do DSP
  ha_settings.*   configurações e calibração dB HL <-> dBFS
  tone_gen.*      gerador de tom pulsado
src/            firmware ESP-IDF
  audio_io.*      I2S full-duplex
  audio_engine.*  task de áudio em tempo real (core 1)
  audiometry_runner.*  temporização da audiometria
  input.*         encoder (PCNT) + botão
  webui.*, web/index.html
test/test_hacore  testes Unity
docs/           guia de hardware e referências
```

## Verificação

1. **Passthrough e latência**: com audiograma vazio o ganho é plano (inserção 0 dB). Grave ao mesmo tempo um estalo perto do microfone e a saída do fone (placa de som ou osciloscópio) e meça o atraso. A meta é menos de 5 ms.
2. **Resposta**: use "Tom de teste" em cada frequência e compare a saída com a curva da página.
3. **Segurança**: com os fones longe dos ouvidos, bata palmas perto do microfone com volume máximo e confirme que a saída não passa do MPO.
4. **Audiometria**: uma pessoa com audição normal deve ficar entre 0 e 20 dB HL (após a calibração biológica), e o reteste de 1 kHz deve ficar dentro de ±5 dB.

## Referências

As fontes da prescrição, do procedimento de audiometria e dos níveis de referência estão em [docs/references.md](docs/references.md).

## Como contribuir

Veja o [CONTRIBUTING.md](CONTRIBUTING.md) e o [Código de Conduta](CODE_OF_CONDUCT.md). Para reportar um problema de segurança, veja o [SECURITY.md](SECURITY.md).

## Licença

O código está sob [Apache-2.0](LICENSE). A documentação, incluindo este README e os arquivos em `docs/`, está sob [CC BY 4.0](docs/LICENSE). Veja o [THIRD_PARTY.md](THIRD_PARTY.md) para as dependências.
