
dwmstatus: status.c
	gcc -o dwmstatus status.c -lX11

clean:
	rm dwmstatus
