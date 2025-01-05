// var gateway = `ws://${window.location.hostname}/ws`;
var gateway = `ws://192.168.1.190/ws`;
var motor_state;
var websocket;

window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
    initInputs();
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    // For now there's no actual action associated with the "state" request, but that simply
    // triggers the ESP to send back the current state of the motor.
    websocket.send('state');
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    motor_state = JSON.parse(event.data);
    if (motor_state.button_pressed == 1) {
        document.getElementById('lockState').innerText = 'Porta chiusa/Door is closed';
        document.getElementById('lockStateIcon').className = 'fas fa-lock';
        document.getElementById('lockStateDirection').className = 'fas';
    } else {
        document.getElementById('lockState').innerText = 'Porta aperta/Door is open';
        document.getElementById('lockStateIcon').className = 'fas fa-unlock';
        document.getElementById('lockStateDirection').className = 'fas';
    }
    if (motor_state.motor_turning == 1 && motor_state.motor_turning_up == 1) {
        document.getElementById('lockState').innerText = 'Porta in apertura/Door is opening';
        document.getElementById('lockStateIcon').className = 'fas fa-cog fa-spin';
        document.getElementById('lockStateDirection').className = 'fas fa-arrow-up';
    } else if (motor_state.motor_turning == 1 && motor_state.motor_turning_up == 0) {
        document.getElementById('lockState').innerText = 'Porta in chiusura/Door is closing';
        document.getElementById('lockStateIcon').className = 'fas fa-cog fa-spin';
        document.getElementById('lockStateDirection').className = 'fas fa-arrow-down';
    }

    // document.getElementById('motor_turning').innerText = motor_state.motor_turning;
    // document.getElementById('button_pressed').innerText = motor_state.button_pressed;
    // document.getElementById('motor_turning_up').innerText = motor_state.motor_turning_up;
}

function initInputs() {
    document.getElementById('upButton').addEventListener('click', onUpRequest);
    document.getElementById('downButton').addEventListener('click', onDownRequest);
}

function onUpRequest(event) {
    websocket.send('upButton');
}
function onDownRequest(event) {
    websocket.send('downButton');
}
