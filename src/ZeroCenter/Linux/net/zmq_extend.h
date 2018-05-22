#pragma once
#ifndef _ZMQ_EXTEND_H_
#define _ZMQ_EXTEND_H_
#include "net_default.h"
#include "net_command.h"
#include "zero_config.h"
namespace agebull
{
	namespace zmq_net
	{

		/**
		*\brief 广播内容(异步)
		*/
		bool monitor_async(string publiher, string state, string content);
		/**
		*\brief 广播内容(同步)
		*/
		bool monitor_sync(string publiher, string state, string content);
		


		/**
		* \brief 网络监控
		* \param address
		* \return
		*/
		DWORD zmq_monitor(const char * address);

		/**
		* \brief 关闭套接字
		* \param socket
		* \param addr
		* \return
		*/
		inline void close_res_socket(ZMQ_HANDLE& socket, const char*  addr)
		{
			zmq_unbind(socket, addr);
			zmq_close(socket);
			socket = nullptr;
		}

		/**
		* \brief 关闭套接字
		* \param socket
		* \param addr
		* \return
		*/
		inline void close_req_socket(ZMQ_HANDLE& socket, const char*  addr)
		{
			zmq_disconnect(socket, addr);
			zmq_close(socket);
			socket = nullptr;
		}

		/**
		* \brief 配置ZMQ连接对象
		* \param socket
		* \param name
		* \return
		*/
		void set_sockopt(const ZMQ_HANDLE& socket, const char* name = nullptr);

		/**
		* \brief 生成ZMQ连接对象
		* \param addr
		* \param type
		* \param name
		* \return
		*/
		inline ZMQ_HANDLE create_req_socket(const char* addr, int type, const char* name)
		{
			const ZMQ_HANDLE socket = zmq_socket(get_zmq_context(), type);
			if (socket == nullptr)
			{
				return nullptr;
			}
			set_sockopt(socket, name);
			if (zmq_connect(socket, addr) >= 0)
				return socket;
			zmq_close(socket);
			return nullptr;
		}

		/**
		* \brief 生成ZMQ连接对象
		* \param addr
		* \param type
		* \param name
		* \return
		*/
		inline ZMQ_HANDLE create_res_socket(const char* addr, int type, const char* name)
		{
			const ZMQ_HANDLE socket = zmq_socket(get_zmq_context(), type);
			if (socket == nullptr)
			{
				return nullptr;
			}
			set_sockopt(socket, name);
			if (zmq_bind(socket, addr) >= 0)
				return socket;
			zmq_close(socket);
			return nullptr;
		}

		/**
		* \brief 生成用于TCP的套接字
		*/
		bool set_tcp_nodelay(ZMQ_HANDLE socket);

		/**
		* \brief 生成用于TCP的套接字
		*/
		inline ZMQ_HANDLE create_res_socket_tcp(const char* name, int type, int port)
		{
			char host[MAX_PATH];
			sprintf(host, "tcp://*:%d", port);
			const ZMQ_HANDLE socket = create_res_socket(host, type, name);
			if (socket == nullptr)
			{
				return nullptr;
			}
			if (!set_tcp_nodelay(socket))
				log_error2("socket(%s) option TCP_NODELAY bad:%s", host, zmq_strerror(zmq_errno()));
			return socket;
		}
		/**
		* \brief 生成用于TCP的套接字
		*/
		inline ZMQ_HANDLE create_req_socket_tcp(const char* name, int type, int port)
		{
			char host[MAX_PATH];
			sprintf(host, "tcp://*:%d", port);
			const ZMQ_HANDLE socket = create_req_socket(host, type, name);
			if (socket == nullptr)
			{
				return nullptr;
			}
			if (!set_tcp_nodelay(socket))
				log_error2("socket(%s) option TCP_NODELAY bad:%s", host, zmq_strerror(zmq_errno()));
			return socket;
		}
		/**
		* \brief 生成用于本机调用的套接字
		*/
		inline ZMQ_HANDLE create_req_socket_ipc(const char* name, int type = ZMQ_REQ)
		{
			char address[MAX_PATH];
			if (config::get_global_bool("use_ipc_protocol"))
				sprintf(address, "ipc:///usr/zero/%s.ipc", name);
			else
				sprintf(address, "inproc://%s", name);
			return create_req_socket(address, type, name);
		}
		/**
		* \brief 生成用于本机调用的套接字
		*/
		inline ZMQ_HANDLE create_res_socket_ipc(const char* name, int type = ZMQ_REQ)
		{
			char address[MAX_PATH];
			if (config::get_global_bool("use_ipc_protocol"))
				sprintf(address, "ipc:///usr/zero/%s.ipc", name);
			else
				sprintf(address, "inproc://%s", name);
			return create_res_socket(address, type, name);
		}
		/**
		* \brief 接收
		*/
		inline zmq_socket_state recv(ZMQ_HANDLE socket, sharp_char& data, int flag = 0)
		{
			//接收命令请求
			zmq_msg_t msg_call;
			int state = zmq_msg_init(&msg_call);
			if (state < 0)
			{
				return zmq_socket_state::NoBufs;
			}
			state = zmq_msg_recv(&msg_call, socket, flag);
			if (state < 0)
			{
				return check_zmq_error();
			}
			data = msg_call;
			zmq_msg_close(&msg_call);
			int more;
			size_t size = sizeof(int);
			zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &size);
			return more == 0 ? zmq_socket_state::Succeed : zmq_socket_state::More;
		}

		/**
		* \brief 接收
		*/
		inline zmq_socket_state recv(ZMQ_HANDLE socket, vector<sharp_char>& ls, int flag = 0)
		{
			size_t size = sizeof(int);
			int more;
			do
			{
				zmq_msg_t msg;
				int re = zmq_msg_init(&msg);
				if (re < 0)
				{
					return zmq_socket_state::NoBufs;
				}
				re = zmq_msg_recv(&msg, socket, flag);
				if (re < 0)
				{
					return check_zmq_error();
				}
				if (re == 0)
					ls.emplace_back();
				else
					ls.emplace_back(msg);
				zmq_msg_close(&msg);
				zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &size);
			} while (more != 0);
			return zmq_socket_state::Succeed;
		}

		/**
		* \brief 发送最后一帧
		*/
		inline zmq_socket_state send_late(ZMQ_HANDLE socket, const char* string)
		{
			const int state = zmq_send(socket, string, strlen(string), ZMQ_DONTWAIT);
			if (state < 0)
			{
				return check_zmq_error();
			}
			return zmq_socket_state::Succeed;
		}

		/**
		* \brief 发送帧
		*/
		inline zmq_socket_state send_more(ZMQ_HANDLE socket, const char* string)
		{
			const int state = zmq_send(socket, string, strlen(string), ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			return zmq_socket_state::Succeed;
		}
		/**
		* \brief 发送帧
		*/
		inline zmq_socket_state send_addr(ZMQ_HANDLE socket, const char* addr)
		{
			int state = zmq_send(socket, addr, strlen(addr), ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			state = zmq_send(socket, "", 0, ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			return zmq_socket_state::Succeed;
		}
		/**
		* \brief 发送帧
		*/
		inline zmq_socket_state send_status(ZMQ_HANDLE socket, const char* addr, const char* status)
		{
			sharp_char d(3);
			char buf[3];
			buf[0] = 1;
			buf[1] = ZERO_FRAME_STATUS;
			buf[2] = ZERO_FRAME_END;
			int state = zmq_send(socket, addr, strlen(addr), ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			state = zmq_send(socket, "", 0, ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			state = zmq_send(socket, buf, 3, ZMQ_SNDMORE);
			if (state < 0)
			{
				return check_zmq_error();
			}
			state = zmq_send(socket, status, strlen(status), ZMQ_DONTWAIT);
			if (state < 0)
			{
				return check_zmq_error();
			}
			return zmq_socket_state::Succeed;
		}

		/**
		* \brief 发送
		*/
		inline zmq_socket_state send(ZMQ_HANDLE socket, vector<sharp_char>::iterator& iter, const vector<sharp_char>::iterator& end)
		{
			while (iter != end)
			{
				const int state = zmq_send(socket, iter->operator*(), iter->size(), ZMQ_SNDMORE);
				if (state < 0)
				{
					return check_zmq_error();
				}
				++iter;
			}
			return send_late(socket, "");
		}
		/**
		* \brief 发送
		*/
		inline zmq_socket_state send(ZMQ_HANDLE socket, vector<sharp_char>& ls, int first_index = 0)
		{
			size_t idx = first_index;
			for (; idx < ls.size() - 1; idx++)
			{
				const int state = zmq_send(socket, *ls[idx], ls[idx].size(), ZMQ_SNDMORE);
				if (state < 0)
				{
					return check_zmq_error();
				}
			}
			return send_late(socket, *ls[idx]);
		}
		/**
		* \brief 发送
		*/
		inline zmq_socket_state send(ZMQ_HANDLE socket, vector<string>& ls, int first_index = 0)
		{
			size_t idx = first_index;
			for (; idx < ls.size() - 1; idx++)
			{
				const int state = zmq_send(socket, ls[idx].c_str(), ls[idx].length(), ZMQ_SNDMORE);
				if (state < 0)
				{
					return check_zmq_error();
				}
			}
			return send_late(socket, ls[idx].c_str());
		}


	}
}

#endif//!_ZMQ_EXTEND_H_
