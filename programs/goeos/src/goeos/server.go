package main

import (
	"crypto/tls"
	"fmt"
	"thrift"
	"rpc"
	"context"
	"unsafe"
	"encoding/binary"
)

// #cgo CFLAGS: -I/Users/newworld/dev/pyeos/programs/goeos/include
// #cgo LDFLAGS: -L/Users/newworld/dev/pyeos/build/programs/goeos /Users/newworld/dev/pyeos/build/programs/goeos/libgoeos.dylib
// #include <goeos.h>
// #include <stdlib.h>
import "C"

type RpcServiceImpl struct {
}

func (this *RpcServiceImpl) FunCall(ctx context.Context, callTime int64, funCode string, paramMap map[string]string) (r []string, err error) {
//	fmt.Println("-->FunCall:", callTime, funCode, paramMap)
	for k, v := range paramMap {
		r = append(r, k+v)
	}
	return
}

func (p *RpcServiceImpl) ReadAction(ctx context.Context) (r []byte, err error) {
    var buffer [256]byte
    ret  := 0//:= C.read_action((*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
    return buffer[:ret], nil
}

// Parameters:
//  - Scope
//  - Table
//  - Payer
//  - ID
//  - Buffer
func (p *RpcServiceImpl) DbStoreI64(ctx context.Context, scope int64, table int64, payer int64, id int64, buffer []byte) (r int32, err error) {
    ret := C.db_store_i64(C.uint64_t(scope), C.uint64_t(table), C.uint64_t(payer), C.uint64_t(id), (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
    return int32(ret), nil
}

// Parameters:
//  - Itr
//  - Payer
//  - Buffer
func (p *RpcServiceImpl) DbUpdateI64(ctx context.Context, itr int32, payer int64, buffer []byte) (err error) {
    C.db_update_i64( C.int(itr), C.uint64_t(payer), (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)));
    return nil
}

// Parameters:
//  - Itr
func (p *RpcServiceImpl) DbRemoveI64(ctx context.Context, itr int32) (err error) {
    C.db_remove_i64(C.int(itr));
    return nil
}

// Parameters:
//  - Itr
func (p *RpcServiceImpl) DbGetI64(ctx context.Context, itr int32) (r []byte, err error) {
    var buffer [256]byte
    ret := C.db_get_i64(C.int(itr), (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
    return buffer[:ret], nil
}

func Int64ToBytes(i uint64) []byte {
    var buf = make([]byte, 8)
    binary.BigEndian.PutUint64(buf, uint64(i))
    return buf
}

func BytesToInt64(buf []byte) uint64 {
    return uint64(binary.BigEndian.Uint64(buf))
}

// Parameters:
//  - Itr
func (p *RpcServiceImpl) DbNextI64(ctx context.Context, itr int32) (r *rpc.Result_, err error) {
    var primary uint64
    var result rpc.Result_
    ret := C.db_next_i64( C.int(itr), (*C.uint64_t)(&primary) )
    result.Status = int32(ret)
    result.Value =  Int64ToBytes(primary)
    return &result, nil;
}

// Parameters:
//  - Itr
func (p *RpcServiceImpl) DbPreviousI64(ctx context.Context, itr int32) (r *rpc.Result_, err error) {
    var primary uint64
    var result rpc.Result_

    ret := C.db_previous_i64(C.int(itr), (*C.uint64_t)(&primary))
    result.Status = int32(ret)
    result.Value =  Int64ToBytes(primary)
    return &result, nil
}

// Parameters:
//  - Code
//  - Scope
//  - Table
//  - ID
func (p *RpcServiceImpl) DbFindI64(ctx context.Context, code int64, scope int64, table int64, id int64) (r int32, err error) {
    ret := C.db_find_i64(C.uint64_t(code), C.uint64_t(scope), C.uint64_t(table), C.uint64_t(id))
    return int32(ret), nil
}


// Parameters:
//  - Code
//  - Scope
//  - Table
//  - ID
func (p *RpcServiceImpl) DbLowerboundI64(ctx context.Context, code int64, scope int64, table int64, id int64) (r int32, err error) {
    ret := C.db_lowerbound_i64(C.uint64_t(code), C.uint64_t(scope), C.uint64_t(table), C.uint64_t(id))
    return int32(ret), nil
}

// Parameters:
//  - Code
//  - Scope
//  - Table
//  - ID
func (p *RpcServiceImpl) DbUpperboundI64(ctx context.Context, code int64, scope int64, table int64, id int64) (r int32, err error) {
    ret := C.db_upperbound_i64(C.uint64_t(code), C.uint64_t(scope), C.uint64_t(table), C.uint64_t(id))
    return int32(ret), nil
}


// Parameters:
//  - Code
//  - Scope
//  - Table
func (p *RpcServiceImpl) DbEndI64(ctx context.Context, code int64, scope int64, table int64) (r int32, err error) {
    ret := C.db_end_i64(C.uint64_t(code), C.uint64_t(scope), C.uint64_t(table))
    return int32(ret), nil
}


func runServer(transportFactory thrift.TTransportFactory, protocolFactory thrift.TProtocolFactory, addr string, secure bool) error {
	var transport thrift.TServerTransport
	var err error
	if secure {
		cfg := new(tls.Config)
		if cert, err := tls.LoadX509KeyPair("server.crt", "server.key"); err == nil {
			cfg.Certificates = append(cfg.Certificates, cert)
		} else {
			return err
		}
		transport, err = thrift.NewTSSLServerSocket(addr, cfg)
	} else {
		transport, err = thrift.NewTServerSocket(addr)
	}
	
	if err != nil {
		return err
	}
	fmt.Printf("%T\n", transport)

	handler := &RpcServiceImpl{}
	processor := rpc.NewRpcServiceProcessor(handler)

//	handler := NewEoslibServiceHandler()
//	processor := idl.EoslibServiceProcessor(handler)
	server := thrift.NewTSimpleServer4(processor, transport, transportFactory, protocolFactory)

	fmt.Println("Starting the simple server... on ", addr)
	return server.Serve()
}
