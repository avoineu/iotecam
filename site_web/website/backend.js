const express = require('express');
const mqtt = require('mqtt');
const http = require('http');
const socketio = require('socket.io');

const app = express();
const server = http.createServer(app);
const io = socketio(server);

app.use(express.static('public'));

const mqttOptions = {
    clientId: 'device1',
    username: 'catfinder2024@ttn',
    password: 'NNSXS.PE63RP3AXWPMXTBST7YM7MAJE3IBT2GC2RLHXUA.RY3JIODZLYOLE7MXK5CVQCUWA75NPEMPO722QAPIUTPOODVJ3UVA',
    host: 'eu1.cloud.thethings.network',
    port: 1883
};

const client = mqtt.connect('mqtt://eu1.cloud.thethings.network:1883', mqttOptions);

client.on('connect', () => {
    console.log('Connected to TTN via MQTT');
    client.subscribe('v3/catfinder2024@ttn/devices/device1/up', (err) => {
        if (err) {
            console.error('Subscription error:', err);
        } else {
            console.log('Subscribed to TTN device uplink topic');
        }
    });
});

client.on('message', (topic, message) => {
    const ttnData = JSON.parse(message.toString());
    console.log('Received message:', ttnData);

    const decodedPayload = ttnData.uplink_message.decoded_payload;
    console.log('Decoded payload:', decodedPayload);

    io.emit('ttn-data', ttnData);
});

app.use(express.static('public'));

app.get('/', (req, res) => {
    res.sendFile(`${__dirname}/index.html`);
});

const PORT = 3000;
server.listen(PORT, () => {
    console.log('Server running on port ${PORT}');
});