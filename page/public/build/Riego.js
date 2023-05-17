var Riego, Estado, Hora, Tiempo, Dias, Programacion;

var connection = new WebSocket("ws://riego.local:81/", ["arduino"]);
// var connection = new WebSocket('ws://45.174.63.6:81/', ['arduino']);

connection.onopen = function () {
	console.log("Connected...");
	console.log(document.title);
	connection.send(document.title);
};
connection.onerror = function (error) {
	console.log("WebSocket Error ", error);
};

// <#PRG#R1E-1R1H-12:00R1T-05R1D-1234567R2E-0R2H-12:00R2T-05R2D-0000000R3E-0R3H-12:00R3T-05R3D-0000000R4E-0R4H-12:00R4T-05R4D-0000000>
connection.onmessage = function (e) {
	console.log("Server: ", e.data);
	document.getElementById("overlay").hidden = true;
	switch (e.data.substr(2, 3)) {
		case "PRG":
			Programacion = e.data;
			// Riego = parseInt(document.title.substr(6)) - 1;
			// console.log(Riego);
			Riego = 1;
			Estado = e.data.substr(10, 1);
			console.log("Estado: ", Estado);
			Hora = e.data.substr(10 + 5, 5);
			console.log("Hora: ", Hora);
			Tiempo = e.data.substr(10 + 14, 2);
			console.log("Tiempo: ", Tiempo);
			Dias = e.data.substr(10 + 20, 7);
			console.log("Dias: ", Dias);

			Activar(Estado);
			document.getElementById("tiempo").value = Tiempo;
			if (parseInt(Hora.substr(0, 2)) > 12) {
				document.getElementById("hora").value = (
					parseInt(Hora.substr(0, 2)) - 12
				)
					.toString()
					.padStart(2, "0");
				document.getElementById("ampm").value = "PM";
			} else {
				document.getElementById("hora").value = Hora.substr(0, 2);
				document.getElementById("ampm").value = "AM";
			}
			document.getElementById("minutos").value = Hora.substr(3, 2);

			for (let i = 0; i <= 6; i++) {
				if (Dias[i] === "0") {
					document.getElementById("D" + i).hidden = true;
				} else {
					document.getElementById("D" + i).hidden = false;
				}
			}

			break;
	}
};

connection.onclose = function () {
	console.log("WebSocket connection closed");
	document.getElementById("overlay").hidden = false;
};

document.getElementById("submit").addEventListener("click", function (event) {
	event.preventDefault();
	Tiempo = document.getElementById("tiempo").value;
	let ampm = document.getElementById("ampm").value;

	if (
		ampm === "PM" &&
		document.getElementById("hora").value !== "12" &&
		document.getElementById("hora").value !== "00"
	) {
		Hora =
			(parseInt(document.getElementById("hora").value) + 12).toString() +
			":" +
			document.getElementById("minutos").value;
	} else {
		Hora =
			document.getElementById("hora").value +
			":" +
			document.getElementById("minutos").value;
	}
	console.log("Hora: ", Hora);
	console.log("Tiempo: ", Tiempo);

	console.log("Estado: ", Estado);
	console.log("Dias: ", Dias);
	// <#PRG#R1E-1R1H-12:00R1T-05R1D-1234567R2E-0R2H-12:00R2T-05R2D-0000000R3E-0R3H-12:00R3T-05R3D-0000000R4E-0R4H-12:00R4T-05R4D-0000000>
	Riego = document.title.substr(5);
	console.log(
		"<#PRG#" +
			"R" +
			Riego +
			"E-" +
			Estado +
			"R" +
			Riego +
			"H-" +
			Hora +
			"R" +
			Riego +
			"T-" +
			Tiempo +
			"R" +
			Riego +
			"D-" +
			Dias +
			">"
	);
	connection.send(
		"<#PRG#" +
			"R" +
			Riego +
			"E-" +
			Estado +
			"R" +
			Riego +
			"H-" +
			Hora +
			"R" +
			Riego +
			"T-" +
			Tiempo +
			"R" +
			Riego +
			"D-" +
			Dias +
			">"
	);
});

function Activar(data) {
	if (data === "1") {
		document.getElementById("switch-on").hidden = false;
		document.getElementById("switch-off").hidden = true;
		Estado = "1";
	} else {
		document.getElementById("switch-on").hidden = true;
		document.getElementById("switch-off").hidden = false;
		Estado = "0";
	}
}

function Dia(numero) {
	let Days = Dias;
	Dias = "";
	if (Days[numero] === "0") {
		Dias =
			Days.substr(0, numero) +
			(numero + 1).toString() +
			Days.substr(numero + 1);
		document.getElementById("D" + numero).hidden = false;
	} else {
		Dias = Days.substr(0, numero) + "0" + Days.substr(numero + 1);
		document.getElementById("D" + numero).hidden = true;
	}
}

function Enviar() {
	// <#PRG#R1E-1R1H-12:00R1T-05R1D-1234567R2E-0R2H-12:00R2T-05R2D-0000000R3E-0R3H-12:00R3T-05R3D-0000000R4E-0R4H-12:00R4T-05R4D-0000000>
	Riego = document.title.substr(5);
	console.log("<#PRG#" + "R" + Riego + "E-" + Estado + "R" + Riego + "H-");
	console.log();
	Estado = e.data.substr(10 + 31 * Riego, 1);
	console.log("Estado: ", Estado);
	Hora = e.data.substr(10 + 31 * Riego + 5, 5);
	console.log("Hora: ", Hora);
	Tiempo = e.data.substr(10 + 31 * Riego + 14, 2);
	console.log("Tiempo: ", Tiempo);
	Dias = e.data.substr(10 + 31 * Riego + 20, 7);
	console.log("Dias: ", Dias);
}
