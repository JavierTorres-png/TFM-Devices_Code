import paho.mqtt.client as mqtt_client

BROKER = "localhost"
PORT = 1883
SUBSCRIBE_TOPIC = "telemetry/#"
PUBLISH_TOPIC = "status/device"

def on_connect(client, userdata, flags, reason_code, properties=None):
	print("Succesful connection to broker")

def on_message(client, userdata, msg):
	print(f"Received the topic {msg.topic}: {msg.payload.decode()}")

def init_client():
	client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2)

	client.on_connect = on_connect
	client.on_message = on_message

	client.connect(BROKER, PORT, 60) # 60 for a 60s keep alive
	client.subscribe(SUBSCRIBE_TOPIC)

	client.loop_start() # Wait for messages and call on_message when you receive one. Creates a thread

def publish_message(publisher_client, deviceID, message):
	publisher_client.publish(PUBLISH_TOPIC + deviceID, message)

def init_publisher():
	client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2)
	client.on_connect = on_connect

	client.connect(BROKER, PORT, 60)
	return client
