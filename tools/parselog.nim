# @file parselog.nim
#
# parse WinFsp debug logs
#
# @copyright 2015-2021 Bill Zissimopoulos

import algorithm
import macros
import parseopt
import parseutils
import strformat
import strscans
import strutils
import tables

type
    Req = ref object
        fsname: string
        tid: uint32
        irp: uint64
        op: string
        inform: string
        context: string
        args: seq[(string, string)]
    Rsp = ref object
        fsname: string
        tid: uint32
        irp: uint64
        op: string
        status: uint32
        inform: uint
        context: string
        args: seq[(string, string)]

proc parseAndAddArg(prefix: string, rest: var string, args: var seq[(string, string)]) =
    discard scanf(rest, ",$*", rest)
    discard scanf(rest, "$s$*", rest)
    var n, v: string
    if scanf(rest, "\"$*\"$*", v, rest):
        args.add((prefix & "", v))
    elif scanf(rest, "$w=\"$*\"$*", n, v, rest):
        args.add((prefix & n, v))
    elif scanf(rest, "$w={$*}$*", n, v, rest):
        while "" != v:
            parseAndAddArg(n & ".", v, args)
    elif scanf(rest, "$w=$*,$*", n, v, rest):
        args.add((prefix & n, v))
    elif scanf(rest, "$w=$*$.", n, v):
        rest = ""
        args.add((prefix & n, v))
    elif scanf(rest, "$*,$*", v, rest):
        args.add((prefix & "", v))
    else:
        v = rest
        rest = ""
        args.add((prefix & "", v))

proc parseArgs(rest: var string): seq[(string, string)] =
    while "" != rest:
        parseAndAddArg("", rest, result)

proc parseReq(op, rest: string): Req =
    result = Req(op: op)
    var rest = rest
    var inform: string
    if scanf(rest, "[$+]$s$*", inform, rest):
        result.inform = inform
    var c0, c1: uint64
    if scanf(rest, "${parseHex[uint64]}:${parseHex[uint64]}$*", c0, c1, rest):
        result.context = toHex(c0) & ":" & toHex(c1)
    result.args = parseArgs(rest)

proc parseRsp(op, rest: string): Rsp =
    result = Rsp(op: op)
    var rest = rest
    var status: uint32
    var inform: uint
    if scanf(rest, "IoStatus=${parseHex[uint32]}[${parseUint}]$s$*", status, inform, rest):
        result.status = status
        result.inform = inform
    var c0, c1: uint64
    if scanf(rest, "UserContext=${parseHex[uint64]}:${parseHex[uint64]}$*", c0, c1, rest):
        result.context = toHex(c0) & ":" & toHex(c1)
    result.args = parseArgs(rest)

proc parseLog(path: string, processReq: proc(req: Req), processRsp: proc(rsp: Rsp)) =
    let file = open(path)
    defer: file.close()
    var lineno = 0
    try:
        for line in lines file:
            inc lineno
            var fsname, dir, op, rest: string
            var tid: uint32
            var irp: uint64
            var req: Req
            var rsp: Rsp
            if scanf(line, "$+[TID=${parseHex[uint32]}]:$s${parseHex[uint64]}:$s$+ $*",
                fsname, tid, irp, op, rest):
                dir = op[0..1]
                op = op[2..^1]
                case dir
                of ">>":
                    req = parseReq(op, rest)
                    req.fsname = fsname
                    req.tid = tid
                    req.irp = irp
                    processReq(req)
                of "<<":
                    rsp = parseRsp(op, rest)
                    rsp.fsname = fsname
                    rsp.tid = tid
                    rsp.irp = irp
                    processRsp(rsp)
                else:
                    continue
    except:
        echo &"An exception has occurred while parsing file {path} line {lineno}"
        raise

type
    Stat = ref object
        ototal: int                     # open total
        omulti: int                     # multiplicate open total
        oerror: int                     # open error total
        rtotal: int                     # read total
        rnoaln: int                     # non-aligned read total
        rbytes: uint64                  # read bytes
        rerror: int                     # read error total
        wtotal: int                     # write total
        wnoaln: int                     # non-aligned write total
        wbytes: uint64                  # write bytes
        werror: int                     # write error total
        dtotal: int                     # query directory total
        dbytes: uint64                  # query directory bytes
        derror: int                     # query directory error total
        ptotal: int                     # query directory w/ pattern total
        pbytes: uint64                  # query directory w/ pattern bytes
        perror: int                     # query directory w/ pattern error total
        ocount: int                     # current open count
var
    reqtab = newTable[uint64, Req]()
    filetab = newTable[string, string]()
    stattab = newOrderedTable[string, Stat]()
    aggr = Stat()

proc getArg(args: seq[(string, string)], name: string): string =
    for n, v in items(args):
        if name == n:
            return v

proc processReq(req: Req) =
    reqtab[req.irp] = req
    case req.op
    of "Close":
        var filename: string
        if filetab.pop(req.context, filename):
            var stat = stattab.mgetOrPut(filename, Stat())
            stat.ocount -= 1

proc processRsp(rsp: Rsp) =
    var req: Req
    if reqtab.pop(rsp.irp, req):
        doAssert req.op == rsp.op
        doAssert req.irp == rsp.irp
        case req.op
        of "Create":
            var filename = getArg(req.args, "")
            if "" != filename:
                if 0 == rsp.status:
                    filetab[rsp.context] = filename
                    var stat = stattab.mgetOrPut(filename, Stat())
                    stat.ototal += 1
                    aggr.ototal += 1
                    stat.ocount += 1
                    if 2 == stat.ocount:
                        stat.omulti += 1
                        aggr.omulti += 1
                else:
                    var stat = stattab.mgetOrPut(filename, Stat())
                    stat.oerror += 1
                    aggr.oerror += 1
        of "Read":
            var filename = filetab[req.context]
            var stat = stattab.mgetOrPut(filename, Stat())
            if 0 == rsp.status or 0xC0000011u32 == rsp.status:
                var oarg = getArg(req.args, "Offset")
                var larg = getArg(req.args, "Length")
                var hi, lo: uint32
                var offset: uint64
                var length: uint
                if scanf(oarg, "${parseHex[uint32]}:${parseHex[uint32]}", hi, lo):
                    offset = uint64(hi) * 0x100000000u64 + uint64(lo)
                discard scanf(larg, "${parseUint}", length)
                stat.rtotal += 1
                stat.rbytes += rsp.inform
                aggr.rtotal += 1
                aggr.rbytes += rsp.inform
                if 0 != offset mod 4096 or 0 != length mod 4096:
                    stat.rnoaln += 1
                    aggr.rnoaln += 1
            else:
                stat.rerror += 1
                aggr.rerror += 1
        of "Write":
            var filename = filetab[req.context]
            var stat = stattab.mgetOrPut(filename, Stat())
            if 0 == rsp.status:
                var oarg = getArg(req.args, "Offset")
                var larg = getArg(req.args, "Length")
                var hi, lo: uint32
                var offset: uint64
                var length: uint
                if scanf(oarg, "${parseHex[uint32]}:${parseHex[uint32]}", hi, lo):
                    offset = uint64(hi) * 0x100000000u64 + uint64(lo)
                discard scanf(larg, "${parseUint}", length)
                stat.wtotal += 1
                stat.wbytes += rsp.inform
                aggr.wtotal += 1
                aggr.wbytes += rsp.inform
                if 0 != offset mod 4096 or 0 != length mod 4096:
                    stat.wnoaln += 1
                    aggr.wnoaln += 1
            else:
                stat.werror += 1
                aggr.werror += 1
        of "QueryDirectory":
            var filename = filetab[req.context]
            var stat = stattab.mgetOrPut(filename, Stat())
            var pattern = getArg(req.args, "Pattern")
            if "NULL" == pattern:
                if 0 == rsp.status:
                    stat.dtotal += 1
                    stat.dbytes += rsp.inform
                    aggr.dtotal += 1
                    aggr.dbytes += rsp.inform
                else:
                    stat.derror += 1
                    aggr.derror += 1
            else:
                if 0 == rsp.status:
                    stat.ptotal += 1
                    stat.pbytes += rsp.inform
                    aggr.ptotal += 1
                    aggr.pbytes += rsp.inform
                else:
                    stat.perror += 1
                    aggr.perror += 1

macro identName(n: untyped): untyped =
    result = n.strVal.newLit

template dumpstat(F: untyped) =
    stattab.sort(proc (x, y: (string, Stat)): int =
        cmp(x[1].F, y[1].F), SortOrder.Descending)
    var width, rows = 0
    for filename, stat in stattab.pairs:
        if 0 == width:
            var s = identName(F).toUpperAscii()
            width = len($aggr.F)
            if width < len(s):
                width = len(s)
            var f: string
            formatValue(f, s, ">" & $width)
            echo f, "  PER% FILENAME"
        var c0, c1: string
        formatValue(c0, stat.F, $width)
        if 0 != aggr.F:
            formatValue(c1, 100.0 * float(stat.F) / float(aggr.F), "5.1f")
        else:
            c1 = "     "
        echo c0, " ", c1, " ", filename
        inc rows
        if opttop == rows:
            break
    var c0: string
    formatValue(c0, aggr.F, $width)
    echo c0, " 100.0 TOTAL"

proc main =
    var filenames: seq[string]
    var optstat: seq[string]
    var opttop = 0
    for kind, key, val in getopt(shortNoVal = {'Z'}, longNoVal = @["Zoo"]):
        case kind
        of cmdShortOption, cmdLongOption:
            case key
            of "stat":
                optstat.add(val)
            of "n":
                opttop = parseInt(val)
        of cmdArgument:
            filenames.add(key)
        else:
            discard
    if 0 == len(optstat):
        optstat.add("ototal")

    if 0 == len(filenames):
        stderr.writeLine("usage: parselog [-nNN] [--stat ototal|rtotal|wtotal|dtotal|...] file...")
        quit(2)

    for filename in filenames:
        parseLog filename, processReq, processRsp

    for s in optstat:
        case s
        of "ototal":
            dumpstat ototal
        of "omulti":
            dumpstat omulti
        of "oerror":
            dumpstat oerror
        of "rtotal":
            dumpstat rtotal
        of "rnoaln":
            dumpstat rnoaln
        of "rbytes":
            dumpstat rbytes
        of "rerror":
            dumpstat rerror
        of "wtotal":
            dumpstat wtotal
        of "wnoaln":
            dumpstat wnoaln
        of "wbytes":
            dumpstat wbytes
        of "werror":
            dumpstat werror
        of "dtotal":
            dumpstat dtotal
        of "dbytes":
            dumpstat dbytes
        of "derror":
            dumpstat derror
        of "ptotal":
            dumpstat ptotal
        of "pbytes":
            dumpstat pbytes
        of "perror":
            dumpstat perror
        echo ""

when isMainModule:
    main()
