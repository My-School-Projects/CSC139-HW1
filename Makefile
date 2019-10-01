all :
	make producer && make consumer

producer : producer.c
	gcc producer.c -lrt -o producer

consumer : consumer.c
	gcc consumer.c -lrt -o consumer

clean :
	rm producer consumer
