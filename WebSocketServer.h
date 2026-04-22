///////////////////////////////////////////////////////////////////////////////
// AOA DF 2m — Angle-of-Arrival Direction Finder for 2m Amateur Radio Band
// File:    WebSocketServer.h
// Author:  Justin Crooks
// Copyright (C) 2025  Justin Crooks
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
///////////////////////////////////////////////////////////////////////////////
// WebSocketServer.h
//
// Self-contained, header-only WebSocket broadcast server for DF2mG.
// Reads phaseDelta, angleOfArrival, and RFFreqPlot from CRadio::myStatus
// and broadcasts them as JSON to index.html at 10 Hz.
//
// Integration — three steps in wx1.cpp:
//   1. Add  #include "WebSocketServer.h"  after the existing includes.
//   2. Add  WebSocketServer* m_wsServer = nullptr;  to MyFrame (in frame1.h, or as a local member).
//   3. In the MyFrame constructor, after  myRadio = new CRadio() :
//          m_wsServer = new WebSocketServer(myRadio);
//          m_wsServer->Start();
//
// ws2_32.lib is linked automatically via #pragma comment below.
// No other build-system changes are required.

#pragma once
#pragma comment(lib, "ws2_32.lib")

// winsock2.h must come before windows.h.
// wx/wx.h includes windows.h with WIN32_LEAN_AND_MEAN (no winsock.h), so
// including winsock2.h here is safe as long as this header is included after wx/wx.h.
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "CRadio.h"

// ============================================================================
//  Internal helpers — wrapped in an anonymous namespace to avoid ODR issues
//  if this header is ever included in more than one translation unit.
// ============================================================================
namespace ws_detail {

// ---- SHA-1 (RFC 3174) — used only for the HTTP upgrade handshake -----------

inline void sha1_block(uint32_t h[5], const uint8_t b[64])
{
    auto rol = [](uint32_t v, int n){ return (v << n) | (v >> (32 - n)); };
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(b[i*4])<<24)|(uint32_t(b[i*4+1])<<16)|
               (uint32_t(b[i*4+2])<<8)|uint32_t(b[i*4+3]);
    for (int i = 16; i < 80; i++)
        w[i] = rol(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);

    uint32_t a=h[0], bb=h[1], c=h[2], d=h[3], e=h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if      (i<20){ f=(bb&c)|(~bb&d);       k=0x5A827999u; }
        else if (i<40){ f=bb^c^d;                k=0x6ED9EBA1u; }
        else if (i<60){ f=(bb&c)|(bb&d)|(c&d);  k=0x8F1BBCDCu; }
        else          { f=bb^c^d;                k=0xCA62C1D6u; }
        uint32_t t=rol(a,5)+f+e+k+w[i];
        e=d; d=c; c=rol(bb,30); bb=a; a=t;
    }
    h[0]+=a; h[1]+=bb; h[2]+=c; h[3]+=d; h[4]+=e;
}

inline void sha1(const uint8_t* data, size_t len, uint8_t out[20])
{
    uint32_t h[5]={0x67452301u,0xEFCDAB89u,0x98BADCFEu,0x10325476u,0xC3D2E1F0u};
    uint64_t bits = uint64_t(len)*8;
    size_t i=0;
    for (; i+64<=len; i+=64) sha1_block(h, data+i);
    uint8_t pad[128]{};
    size_t rem=len-i;
    std::memcpy(pad, data+i, rem);
    pad[rem]=0x80;
    size_t pl=(rem<56)?64:128;
    for (int j=0;j<8;j++) pad[pl-1-j]=uint8_t(bits>>(j*8));
    sha1_block(h, pad);
    if (pl==128) sha1_block(h, pad+64);
    for (int j=0;j<5;j++){
        out[j*4+0]=(h[j]>>24)&0xFF; out[j*4+1]=(h[j]>>16)&0xFF;
        out[j*4+2]=(h[j]>>8)&0xFF;  out[j*4+3]=h[j]&0xFF;
    }
}

// ---- Base64 ----------------------------------------------------------------

inline std::string base64_encode(const uint8_t* d, size_t n)
{
    static const char T[]=
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; o.reserve(((n+2)/3)*4);
    for (size_t i=0;i<n;i+=3){
        uint32_t v=uint32_t(d[i])<<16;
        if (i+1<n) v|=uint32_t(d[i+1])<<8;
        if (i+2<n) v|=d[i+2];
        o+=T[(v>>18)&63]; o+=T[(v>>12)&63];
        o+=(i+1<n)?T[(v>>6)&63]:'=';
        o+=(i+2<n)?T[v&63]:'=';
    }
    return o;
}

// ---- WebSocket protocol helpers --------------------------------------------

inline std::string ws_accept(const std::string& key)
{
    std::string s=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t hash[20];
    sha1(reinterpret_cast<const uint8_t*>(s.data()), s.size(), hash);
    return base64_encode(hash,20);
}

inline std::string parse_ws_key(const std::string& req)
{
    const std::string needle="Sec-WebSocket-Key:";
    auto pos=req.find(needle);
    if (pos==std::string::npos) return {};
    pos+=needle.size();
    while (pos<req.size()&&req[pos]==' ') pos++;
    auto end=req.find("\r\n",pos);
    return req.substr(pos, end-pos);
}

inline std::string recv_headers(SOCKET sock)
{
    std::string buf; char c;
    while (buf.size()<8192){
        int r=recv(sock,&c,1,0);
        if (r<=0) break;
        buf+=c;
        if (buf.size()>=4&&buf.substr(buf.size()-4)=="\r\n\r\n") break;
    }
    return buf;
}

inline bool ws_send_text(SOCKET sock, const std::string& msg)
{
    size_t len=msg.size();
    uint8_t hdr[10]; int hlen=0;
    hdr[hlen++]=0x81;
    if      (len<126)  { hdr[hlen++]=uint8_t(len); }
    else if (len<65536){ hdr[hlen++]=126; hdr[hlen++]=uint8_t(len>>8); hdr[hlen++]=uint8_t(len); }
    else               { hdr[hlen++]=127; for(int i=7;i>=0;i--) hdr[hlen++]=uint8_t(len>>(i*8)); }
    if (send(sock,reinterpret_cast<const char*>(hdr),hlen,0)<=0) return false;
    if (send(sock,msg.data(),int(len),0)<=0)                     return false;
    return true;
}

} // namespace ws_detail

// ============================================================================
//  WebSocketServer
// ============================================================================

class WebSocketServer
{
public:
    // port: the WebSocket port index.html connects to (default 8080)
    explicit WebSocketServer(CRadio* radio, int port = 8080)
        : m_radio(radio), m_port(port), m_running(false) {}

    ~WebSocketServer() { Stop(); }

    // Start accept thread and data-broadcast thread.
    // Safe to call immediately after CRadio is constructed — the server will
    // wait for myStatus data to become valid before broadcasting.
    void Start()
    {
        if (m_running.exchange(true)) return; // already running

        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);

        m_serverSock = socket(AF_INET, SOCK_STREAM, 0);
        int yes=1;
        setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&yes), sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(m_port);
        bind(m_serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        listen(m_serverSock, 8);

        // Accept thread
        m_acceptThread = std::thread([this]{
            while (m_running) {
                SOCKET client = accept(m_serverSock, nullptr, nullptr);
                if (client == INVALID_SOCKET) break;
                std::thread([this, client]{ clientSession(client); }).detach();
            }
        });

        // Broadcast thread — 10 Hz
        m_dataThread = std::thread([this]{
            using clock = std::chrono::steady_clock;
            auto next   = clock::now();
            const auto  interval = std::chrono::microseconds(100'000); // 100 ms = 10 Hz

            while (m_running) {
                next += interval;
                std::this_thread::sleep_until(next);

                if (!m_radio->connected || !m_radio->myStatus) continue;

                // Read shared state — no lock, same pattern as the existing 125 ms wxTimer.
                float b2    = m_radio->myStatus->phaseDelta;
                float b1    = m_radio->myStatus->angleOfArrival;
                float freq  = m_radio->myStatus->RXFreq;
                float line[256];
                std::memcpy(line, m_radio->myStatus->RFFreqPlot, sizeof(line));

                broadcast(buildJson(b1, b2, freq, line));
            }
        });
    }

    bool IsRunning() const { return m_running.load(); }

    void Stop()
    {
        if (!m_running.exchange(false)) return;
        closesocket(m_serverSock);
        if (m_acceptThread.joinable()) m_acceptThread.join();
        if (m_dataThread.joinable())   m_dataThread.join();
        WSACleanup();
    }

private:
    // ---- Client session ----------------------------------------------------

    void clientSession(SOCKET sock)
    {
        std::string req = ws_detail::recv_headers(sock);
        std::string key = ws_detail::parse_ws_key(req);
        if (key.empty()) { closesocket(sock); return; }

        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + ws_detail::ws_accept(key) + "\r\n\r\n";
        send(sock, resp.data(), int(resp.size()), 0);

        addClient(sock);

        // Drain frames from the browser until it disconnects
        char buf[256];
        while (recv(sock, buf, sizeof(buf), 0) > 0) {}

        removeClient(sock);
        closesocket(sock);
    }

    // ---- Client list -------------------------------------------------------

    void addClient(SOCKET s)
    {
        std::lock_guard<std::mutex> g(m_mtx);
        m_clients.push_back(s);
    }

    void removeClient(SOCKET s)
    {
        std::lock_guard<std::mutex> g(m_mtx);
        m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), s),
                        m_clients.end());
    }

    void broadcast(const std::string& msg)
    {
        std::lock_guard<std::mutex> g(m_mtx);
        for (SOCKET s : m_clients) ws_detail::ws_send_text(s, msg);
    }

    // ---- JSON builder ------------------------------------------------------

    static std::string buildJson(float bearing1, float bearing2, float rxfreq, const float line[256])
    {
        std::ostringstream o;
        o << std::fixed; o.precision(3);
        o << "{\"bearing1\":" << bearing1
          << ",\"rxfreq\":"   << rxfreq;
        o.precision(2);
        o << ",\"line\":[";
        for (int i = 0; i < 256; i++) {
            if (i) o << ',';
            o << line[i];
        }
        o << "]}";
        return o.str();
    }

    // ---- Members -----------------------------------------------------------

    CRadio*               m_radio;
    int                   m_port;
    std::atomic<bool>     m_running;
    SOCKET                m_serverSock = INVALID_SOCKET;
    std::mutex            m_mtx;
    std::vector<SOCKET>   m_clients;
    std::thread           m_acceptThread;
    std::thread           m_dataThread;
};
