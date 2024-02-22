start: image clean
	docker run -v "$$(pwd)/..":/ns3/assets -v "$$(pwd)/ns-3.40/src/p4-switch":/ns3/ns-3.40/src/p4-switch -d -ti --name ns3bmv2 ns3bmv2
	docker exec ns3bmv2 /bin/bash -c 'cp -r /ns3/assets/p4src /ns3/ns-3.40/src/p4-switch/examples'
	docker exec ns3bmv2 /bin/bash -c 'cd /ns3/ns-3.40/src/p4-switch/examples/p4src && p4c -o ../livelive_build ./srv6_livelive.p4'
	docker exec ns3bmv2 /bin/bash -c 'cd /ns3/ns-3.40/src/p4-switch/examples/p4src && p4c -o ../forward_build ./srv6_forward.p4'

image:
	docker image inspect ns3bmv2 >/dev/null 2>&1 && echo "Image built." || docker build -t ns3bmv2 $$(pwd)/

clean:
	docker rm -f ns3bmv2