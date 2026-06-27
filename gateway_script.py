import client.client_handler as client_mqtt
import azure.azure_handler as azure
import threading
import json
import time

# This function is called whenever an mqtt subscriber receives a message
def on_mqtt_message(client, userdata, msg):
	threading.Thread(target=process_message, args=(msg,), daemon=True).start()

# This function is called to process the message received by the mqtt subscriber
def process_message(msg):
	print(f"Received the topic {msg.topic}: {msg.payload.decode()}")
	# Sanity check
	payload = validate_and_process_payload(msg.payload.decode())
	if (payload is not None):
		device_id = payload["device_id"]

		# Transform payload to what Azure ML expects
		payload = {
				"input_data": {
					"columns": [
						"temperature",
						"humidity",
						"water_level",
						"N",
						"P",
						"K"
					],
					"index": [0],
					"data": [[
						payload["temperature"],
						payload["humidity"],
						payload["water_level"],
						payload["N"],
						payload["P"],
						payload["K"]
					]]
				},
				"params": {}
			}

		# Send payload to Azure ML
		output = azure.send_message_to_azure_ml(payload)
		print(f"Received status code {output.status_code} with content: {output.text}")

		# Send the received status to the client
		if (output.status_code == 200):
			if (output.text == "[11]"):
				message = "{\"water_pump_status\": \"ON\",\n\"fan_status\":\"ON\"}"
			elif (output.text == "[10]"):
				message = "{\"water_pump_status\": \"OFF\",\n\"fan_status\":\"ON\"}"
			elif (output.text == "[01]"):
				message = "{\"water_pump_status\": \"ON\",\n\"fan_status\":\"OFF\"}"
			elif (output.text == "[00]"):
				message = "{\"water_pump_status\": \"OFF\",\n\"fan_status\":\"OFF\"}"
			else:
				print("Unexpected output.text value:" + output.text)
				return
			publisher = client_mqtt.init_publisher()
			client_mqtt.publish_message(publisher, "status/device" + device_id, message)


# This function is used to validate the received payload and turn it into json
def validate_and_process_payload(payload):
	try:
		data = json.loads(payload)

	except json.JSONDecodeError:
		print("Received payload was not json valid and will be discarded")
		return None

	if ("device_id" not in data or "temperature" not in data or
	"humidity" not in data or "water_level" not in data or
	"N" not in data or "P" not in data or "K" not in data):
		print("Message did not contain expected fields")
		return None

	return data

def thread_check_for_update():
	try:
		while True:
			update = azure.check_for_update()
			if (update):
				publisher = client_mqtt.init_publisher()
				client_mqtt.publish_message(publisher, "status/update", "New code update")
			time.sleep(60)
	except KeyboardInterrupt:
		# Print already in main
		pass

def main():
	# Listen to ESP32 data
	client_mqtt.init_client(on_mqtt_message)

	threading.Thread(target=thread_check_for_update, args=(), daemon=True).start()

	try:
		while True:
			time.sleep(50)
	except KeyboardInterrupt:
		print("Terminating execution")

if __name__=="__main__":
	main()
