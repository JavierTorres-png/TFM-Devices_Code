import client.client_handler as client_mqtt

client_mqtt.init_client()
publisher = client_mqtt.init_publisher()
client_mqtt.publish_message(publisher, "8C94DF6C050C", "Hello")
