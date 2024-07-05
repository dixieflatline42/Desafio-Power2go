# ESP32 com MQTT e DHT22

## Desenvolvido por Kevin da Cruz Souto

Este projeto utiliza um ESP32 para ler dados de um sensor DHT22, controlar um LED usando a biblioteca NeoPixel e interagir com um botão utilizando interrupções. Os dados do sensor são publicados via MQTT, e o ESP32 pode receber comandos via MQTT para controlar o estado do LED.

## Visão Geral

O projeto tem como objetivo monitorar a temperatura e umidade usando um sensor DHT22, publicar esses dados via MQTT e controlar um LED RGB através de comandos recebidos via MQTT.

## Como Utilizar

### Passos para Configurar o Cliente MQTT

1. Acesse a URL abaixo para simular o projeto:
   [Simulador Online Wokwi]([http://www.hivemq.com/demos/websocket-client/](https://wokwi.com/projects/402577585675949057))
  
2.  Acesse a URL abaixo e clique no botão conectar:
   [HiveMQ WebSocket Client](http://www.hivemq.com/demos/websocket-client/)

3. Adicione os tópicos de assinatura, um para cada tópico que o ESP32 utiliza:
   - `temp-led-data/#`
   - `led-state-control/#`

4. Experimente publicar no tópico `led-state-control` com as seguintes mensagens:
   - Para ligar o LED com controle pelo botão: `"HIGH_BUTTON"`
   - Para ligar o LED com controle pelo MQTT: `"HIGH_MQTT"`
   - Para desligar o LED: `"LOW"`
   - Para alterar o tempo de piscagem: `"1000"` ou `"300"`
   - Para mudar a cor do LED: `"GREEN"`, `"YELLOW"`, `"RED"`, `"BLUE"`

### Descrição das Funcionalidades

- **Leitura de Sensor DHT22**: O ESP32 lê a temperatura e umidade do sensor DHT22 e publica esses dados via MQTT no tópico `temp-led-data`.
- **Controle de LED RGB**: O LED pode ser controlado manualmente por um botão ou remotamente via comandos MQTT no tópico `led-state-control`.
- **Interação via Botão**: O botão pode ligar/desligar o LED ou alterar seu modo de operação.
- **Publicação MQTT**: Dados de temperatura, umidade e estado do LED são publicados periodicamente via MQTT.
- **Comandos MQTT**: O ESP32 responde a comandos MQTT para controlar o estado do LED (ligar, desligar, alterar cor e tempo de piscagem).

---

