# Segurança

[English](SAFETY.md)

**Este é um projeto experimental. Ele não é um dispositivo médico.** Leia esta página antes de montar ou usar o aparelho. O software é fornecido sem nenhuma garantia; veja a [LICENSE](LICENSE).

## Não é um dispositivo médico

- Não passou por validação clínica, nem por avaliação ou aprovação de nenhum órgão regulador.
- Não substitui audiometria feita por fonoaudiólogo nem aparelho auditivo prescrito.
- O resultado da audiometria não é um diagnóstico. Se você notar perda auditiva, zumbido, dor no ouvido ou tontura, procure um profissional de saúde.

## Risco de dano auditivo

Este aparelho toca som amplificado diretamente nos ouvidos. Som alto pode causar dano auditivo **permanente**.

- Coloque sempre o volume no mínimo antes de pôr os fones e aumente devagar.
- Mantenha o limite de saída (MPO) conservador. Não o aumente sem uma medição calibrada.
- Tire os fones imediatamente se o som for doloroso, desconfortável ou inesperadamente alto.
- Confira o limitador de saída depois de qualquer mudança de firmware ou de configuração, com os fones longe dos ouvidos. Veja "Verificação" no [README](README.pt-BR.md#verificação).
- Um bug no firmware, um erro de ligação ou um fone diferente podem produzir som muito mais alto que o esperado.

## Os valores em SPL dependem de calibração

O firmware aceita `mpo_spl` de até 110 dB SPL. **Os valores em dB SPL e dB HL exibidos só correspondem à realidade depois da calibração com o mesmo fone em uso.** Sem calibração, a saída real pode ser bem maior que o valor exibido. Trocar de fone invalida a calibração. Veja [Calibração](docs/pt-BR/hardware.md#calibração).

## Acesso às configurações sem fio

O modo configuração abre uma rede Wi-Fi cuja senha padrão é pública neste repositório. Enquanto esse modo estiver ativo, qualquer pessoa no alcance que se conectar pode tocar tons de teste e mudar configurações, inclusive o limite de saída. Saia do modo configuração ao terminar e não use os fones com esse modo ativo perto de outras pessoas. Veja o [SECURITY.md](SECURITY.md).

## Baterias

O projeto usa uma célula de íon-lítio 18650, que pode pegar fogo se sofrer curto-circuito, dano físico ou descarga excessiva. Use um módulo de carga com proteção (TP4056 com DW01 ou similar) e não deixe o aparelho carregando sem supervisão. A proteção de bateria baixa do firmware vem **desligada por padrão** e exige o divisor de tensão descrito em [docs/pt-BR/hardware.md](docs/pt-BR/hardware.md).

## Distribuição e regulação

Aparelhos auditivos e audiômetros são produtos regulados em muitos países, por exemplo pela ANVISA no Brasil e pela FDA nos Estados Unidos. Se você vender, doar ou distribuir de outra forma aparelhos montados ou binários do firmware, a responsabilidade por cumprir as regras locais é sua. A licença deste projeto não concede nenhuma aprovação regulatória.
