1.001667	time printf "qdisc add dev lo root netem loss random 10\nqdisc change dev lo root netem loss random 50\n" | sudo tc -batch -
20.001667	time printf "qdisc delete dev lo root" | sudo tc -batch -
