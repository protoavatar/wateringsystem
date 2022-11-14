// var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
// EstadoRiego -> "<#RIE#R1-0R2-0R3-0R4-0RT-0>";

var EstadoRiego = "<#RIE#R1-0R2-0R3-0R4-0RT-0>"

var connection = new WebSocket('ws://balcon.local:81/', ['arduino']);
// var connection = new WebSocket('ws://45.174.63.6:81/', ['arduino']);
connection.onopen = function () {
    connection.send('Index');
};
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
    console.log('Server: ', e.data);
    document.getElementById("overlay").hidden = true;
    switch (e.data.substr(2, 3)) {
        case "RIE":
            EstadoRiego = e.data;
            for (let i = 1; i <= 5; i++) { // Itero sobre todos los riegos para activar el que me llego
                let element = document.getElementById("R" + i);
                if (e.data.substr(9 + (i - 1) * 4, 1) === "1") {
                    element.classList.remove("bg-green-500");
                    element.classList.remove("hover:bg-green-600");
                    element.classList.add("bg-red-500");
                    element.classList.add("hover:bg-red-600");
                    document.getElementById('boton').textContent = "Apagar";
                }
                else {
                    element.classList.remove("bg-red-500");
                    element.classList.remove("hover:bg-red-600");
                    element.classList.add("bg-green-500");
                    element.classList.add("hover:bg-green-600");
                    document.getElementById('boton').textContent = "Encender";
                }
            }
            break;
        case "ACT":
            if (e.data.substr(6, 1) === "1") {
                document.getElementById("switch-on").hidden = false;
                document.getElementById("switch-off").hidden = true;
            } else {
                document.getElementById("switch-on").hidden = true;
                document.getElementById("switch-off").hidden = false;
            }
            break;
    }

}

connection.onclose = function () {
    document.getElementById("overlay").hidden = false;
    console.log('WebSocket connection closed');
};

// EstadoRiego -> "<#RIE#R1-0R2-0R3-0R4-0RT-0>";
function Riego(riego) {
    if (EstadoRiego.substr(25, 1) === "0") {
        switch (riego) {
            case 1:
                if (EstadoRiego.substr(9, 1) === "0") {
                    connection.send("<#RIE#R1-1R2-0R3-0R4-0RT-0>");
                } else {
                    connection.send("<#RIE#R1-0R2-0R3-0R4-0RT-0>")
                }
                break;
            case 2:
                if (EstadoRiego.substr(13, 1) === "0") {
                    connection.send("<#RIE#R1-0R2-1R3-0R4-0RT-0>");
                } else {
                    connection.send("<#RIE#R1-0R2-0R3-0R4-0RT-0>")
                }
                break;
            case 3:
                if (EstadoRiego.substr(17, 1) === "0") {
                    connection.send("<#RIE#R1-0R2-0R3-1R4-0RT-0>");
                } else {
                    connection.send("<#RIE#R1-0R2-0R3-0R4-0RT-0>")
                }
                break;
            case 4:
                if (EstadoRiego.substr(21, 1) === "0") {
                    connection.send("<#RIE#R1-0R2-0R3-0R4-1RT-0>");
                } else {
                    connection.send("<#RIE#R1-0R2-0R3-0R4-0RT-0>")
                }
                break;
        }
    }
}

//Used only if I have several watering valves and want a general enable/disable state for the system
// function Activar(data) {
//     if (data === "1") {
//         document.getElementById("switch-on").hidden = false;
//         document.getElementById("switch-off").hidden = true;
//         connection.send("<#ACT#1>")
//     } else {
//         document.getElementById("switch-on").hidden = true;
//         document.getElementById("switch-off").hidden = false;
//         connection.send("<#ACT#0>")
//     }
// }