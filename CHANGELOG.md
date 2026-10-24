# Changelog

## 0.1.0

Primeira versão.

- Métricas (`ANX_VALUE`, `ANX_VALUE_EVERY`), eventos, estados, cronometragem de seções e logs.
- Relatórios automáticos de loop (frequência, tempo médio/máximo), RAM livre e descartes.
- Handshake com `ANX PING` e desligamento automático por timeout.
- Descarte de frames com buffer cheio, sem travar o loop.
- `ANATIX_DISABLE` para remover a telemetria da compilação.
- Protocolo v1 com checksum ([docs/PROTOCOLO.md](docs/PROTOCOLO.md)).
