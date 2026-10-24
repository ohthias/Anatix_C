# Protocolo Anatix — versão 1

Comunicação serial em texto (ASCII), uma mensagem por linha, terminada em `\n`.
Taxa recomendada: 115200 baud (precisa ser igual dos dois lados).

## Robô → computador

```
$ANX,<tipo>,<millis>,<campos...>*<CK>
```

- `<millis>` é o `millis()` do robô no momento do envio.
- `<CK>` é o XOR de todos os bytes entre `$` e `*` (exclusive), em hexadecimal com 2 dígitos maiúsculos.
- Linhas que não começam com `$ANX,` são saídas comuns do robô (`Serial.print`) e devem ser tratadas como console.
- Frames com checksum inválido devem ser descartados.

| Tipo | Nome | Campos após `<millis>` | Observações |
|---|---|---|---|
| `H` | hello | versão, robô, placa, período_ms | enviado ao conectar e em resposta a `ANX HELLO` |
| `V` | valor | nome, valor | valor decimal; pode ser `nan`, `inf`, `-inf` |
| `E` | evento | nome, dado | dado inteiro (0 se omitido) |
| `S` | estado | máquina, estado | só enviado quando o estado muda; reenviado ao reconectar |
| `T` | tempo | seção, execuções, média_µs, máx_µs | agregado a cada período |
| `L` | loop | loops_por_s, média_µs, máx_µs, ram_livre, descartes | agregado a cada período; `ram_livre = -1` se indisponível |
| `M` | mensagem | texto | o texto pode conter vírgulas (é o último campo) |

Exemplos:

```
$ANX,H,10,1,MeuRobo,atmega328p,250*27
$ANX,V,1520,distancia_cm,23.400*23
$ANX,S,1533,modo,desviando*60
$ANX,T,1750,pid,133,389,430*49
$ANX,L,1750,529,1890,1930,812,0*0A
```

## Computador → robô

| Comando | Efeito |
|---|---|
| `ANX PING` | conecta (na primeira vez) e mantém a conexão viva |
| `ANX HELLO` | pede o frame `H` novamente |
| `ANX STOP` | para de transmitir |
| `ANX RATE <ms>` | muda o período dos frames `L` e `T` (20 a 10000) |
| `ANX RESET` | zera as estatísticas agregadas |

O computador deve enviar `ANX PING` com intervalo menor que o timeout do robô
(2000 ms por padrão; recomendado: a cada 500 ms). Sem PING nesse tempo, o robô
para de transmitir.

Placas como o Uno reiniciam quando a porta é aberta; continue enviando PING até
receber o frame `H`.

## Leitura de referência (pseudocódigo)

```
para cada linha:
    i = posição de "$ANX," na linha; se não houver → console
    j = última posição de "*"
    corpo = linha[i+1 .. j-1]
    se XOR(corpo) != hex(linha[j+1 .. j+2]) → descartar
    campos = corpo separado por ","
    tipo = campos[1], t = campos[2]
```
