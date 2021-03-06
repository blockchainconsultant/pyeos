package main

import (
	"context"
	"crypto/tls"
	"fmt"
	"time"
	"rpc"
	"thrift"
)

var defaultCtx = context.Background()

func currentTimeMillis() int64 {
	return time.Now().UnixNano() / 1000000
}

func handleClient(client *rpc.RpcServiceClient) (err error) {

    start := time.Now().UnixNano()
    count := 20000

    for i:=0;i<count;i++ {
        client.ReadAction(defaultCtx)
    }

    duration := time.Now().UnixNano() - start
    fmt.Println("transactions per second", int64(count)*1e9/duration);

	return nil
}

func runClient(transportFactory thrift.TTransportFactory, protocolFactory thrift.TProtocolFactory, addr string, secure bool) error {
	var transport thrift.TTransport
	var err error
	if secure {
		cfg := new(tls.Config)
		cfg.InsecureSkipVerify = true
		transport, err = thrift.NewTSSLSocket(addr, cfg)
	} else {
		transport, err = thrift.NewTSocket(addr)
	}
	if err != nil {
		fmt.Println("Error opening socket:", err)
		return err
	}
	transport, err = transportFactory.GetTransport(transport)
	if err != nil {
		return err
	}
	defer transport.Close()
	if err := transport.Open(); err != nil {
		return err
	}

	iprot := protocolFactory.GetProtocol(transport)
	oprot := protocolFactory.GetProtocol(transport)
	return handleClient(rpc.NewRpcServiceClient(thrift.NewTStandardClient(iprot, oprot)))
}
