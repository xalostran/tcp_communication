package main

import (
	"fmt"
	"log"
	"net"
	"strings"
	"time"
)

var ledOnTime time.Time
var isLedOn bool

func main() {
	fmt.Println("Starting server...") // Debug-logg

	newIpAndPort := ":4242"
	listener, err := net.Listen("tcp", newIpAndPort)
	if err != nil {
		log.Fatal("Error starting server:", err)
	}

	fmt.Println("Server is running on", newIpAndPort) // Debug-logg
	defer listener.Close()

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Println("Error accepting connection:", err)
			continue
		}
		fmt.Println("Accepted connection from", conn.RemoteAddr()) // Debug-logg
		go handleClient(conn)
	}
}

func combineDistance(message string) float64 {
	var distance float64
	_, err := fmt.Sscanf(message, "Distance: %f cm", &distance)
	if err != nil {
		log.Println("Unintended error while parsing distance: ", err)
	}
	return distance
}

func handLedOn() string {
	if !isLedOn {
		isLedOn = true
		ledOnTime = time.Now()
		fmt.Println("LED is ON")
	}
	return "TURN_ON_LED"
}

func handLedOff() string {
	if isLedOn {
		isLedOn = false
		fmt.Println("LED is OFF")
	}
	return "TURN_OFF_LED"
}

func handleClient(conn net.Conn) {
	defer conn.Close()
	fmt.Println("Connected:", conn.RemoteAddr().String())

	buffer := make([]byte, 1024)
	timeoutDuration := 30 * time.Second // Timeout för att förhindra långvarig inaktivitet

	for {
		// Sätt en deadline för att hantera timeout
		err := conn.SetDeadline(time.Now().Add(timeoutDuration))
		if err != nil {
			log.Println("Error setting deadline:", err)
			return
		}

		// Läs meddelandet från klienten (ESP32)
		n, err := conn.Read(buffer)
		if err != nil {
			if err.Error() == "EOF" {
				fmt.Println("Connection closed by client")
				return
			}
			log.Println("Error reading data:", err)
			return
		}

		var response string // Respons från servern

		if n > 0 {
			message := string(buffer[:n])
			fmt.Println("Received message: ", message)

			// Parsa avståndet och fatta beslut om LED och alarm
			if strings.Contains(message, "Distance:") {
				distance := combineDistance(message)

				if distance >= 0 && distance < 25 {
					response = handLedOn()
				} else {
					response = handLedOff()
				}
			}

			// Hantera alarm baserat på LED-status och tid
			if isLedOn {
				if time.Since(ledOnTime) >= 10*time.Second {
					response += ";TURN_ON_ALARM"
				} else {
					response += ";TURN_OFF_ALARM"
				}
			} else {
				response += ";TURN_OFF_ALARM"
			}
		} else {
			// Om inget meddelande tas emot, skicka NO_ACTION
			response = "NO_ACTION;TURN_OFF_ALARM"
		}

		// Skicka tillbaka responsen till ESP32
		_, err = conn.Write([]byte(response))
		if err != nil {
			log.Println("Error sending data:", err)
			return
		}
		fmt.Println("Sent response: ", response)
	}
}
