#include "json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <openssl/ssl.h>

#define DEFAULT_BUFLEN 16384
using json = nlohmann::json;
using std::string;
std::map<string, string> sourceMap = { {"CAMBRIDGE", "剑桥高阶英汉双解词典"}, {"LONGMAN", "朗文当代高级英语词典"}, {"COLLINS", "柯林斯英汉双解大词典"}, {"ONLINE", "金山词霸"} };

int maxSentence = 2;
string httpsRuest(const char *host, const char *message)
{
    int portno = 443;
    struct hostent *server;
    struct sockaddr_in serv_addr;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server = gethostbyname(host);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    SSL_load_error_strings();
    SSL_library_init();
    SSL_CTX *ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    SSL *conn = SSL_new(ssl_ctx);
    SSL_set_fd(conn, sockfd);
    SSL_connect(conn);

	int recvbuflen = DEFAULT_BUFLEN;
	char recvbuf[DEFAULT_BUFLEN] = { '0' };
	SSL_write(conn, message, (int)strlen(message));
	string response{ "" };
	int iResult;
	do
	{
		do
		{
			iResult = SSL_read(conn, recvbuf, recvbuflen - 1);
			response += recvbuf;
			memset(recvbuf, 0, sizeof(recvbuf));
		} while (SSL_pending(conn) > 0);
	} while (iResult > 0);

    SSL_free(conn);
    close(sockfd);

	return response;
}
void parseBrief(json brief)
{
	if (brief["lemma"].find("relation") != brief["lemma"].end())
	{
		printf("\033[0;31m");
		printf("%s: ", brief["wordOut"].get<string>().c_str());
		printf("\033[0m");
		printf("%s 的 %s\n", brief["lemma"]["lemma"].get<string>().c_str(), brief["lemma"]["relation"].get<string>().c_str());
	}
	else
	{
		printf("\033[0;31m");
		printf("%s:\n", brief["wordOut"].get<string>().c_str());
	}
	if (brief.find("usPron") != brief.end() || brief.find("ukPron") != brief.end())
	{
		// printf("\033[0;32m")
		// printf("  音标\n");
		if (brief.find("usPron") != brief.end())
		{
			printf("\033[0;33m");
			printf("    美音 ");
			printf("\033[0m");
			printf("/%-2s/", brief["usPron"]["ps"].get<string>().c_str());
		}
		if (brief.find("ukPron") != brief.end())
		{
			printf("\033[0;33m");
			printf("    英音 ");
			printf("\033[0m");
			printf("/%-2s/", brief["ukPron"]["ps"].get<string>().c_str());
		}
		printf("\n");
	}
	if (brief.find("chnDefinitions") != brief.end())
	{
		auto chn = brief["chnDefinitions"];
		printf("\033[0;32m");
		printf("中文释义\n");
		for (auto &def : chn)
		{
			if (def.find("pos") != def.end())
			{
				printf("\033[0;33m");
				printf("    %-8s", def["pos"].get<string>().c_str());
				printf("\033[0m");
				printf("%s\n", def["meaning"].get<string>().c_str());
			}
			else
			{
				printf("\033[0m");
				printf("            %s\n", def["meaning"].get<string>().c_str());
			}
		}
	}
	if (brief.find("engDefinitions") != brief.end())
	{
		auto chn = brief["engDefinitions"];
		printf("\033[0;32m");
		printf("英文释义\n");
		for (auto &def : chn)
		{
			if (def.find("pos") != def.end())
			{
				printf("\033[0;33m");
				printf("    %-8s", def["pos"].get<string>().c_str());
				printf("\033[0m");
				printf("%s\n", def["meaning"].get<string>().c_str());
			}
			else
			{
				printf("\033[0m");
				printf("            %s\n", def["meaning"].get<string>().c_str());
			}
		}
	}
}
string parseSource(json sentenceList)
{
	if (sentenceList.find("source") != sentenceList.end())
		return sourceMap[sentenceList["source"].get<string>()];
	else
		return "牛津高阶英汉双解词典";
}
void parseDetail(json detail)
{
	parseBrief(detail["wordBrief"]);
	if (detail.find("sentenceLists") != detail.end())
	{
		auto sentenceLists = detail["sentenceLists"];
		printf("\033[0;32m");
		printf("双语例句\n");
		for (auto &sentenceList : sentenceLists)
		{
			printf("\033[0;33m");
			printf("    %s\n", parseSource(sentenceList).c_str());
			printf("\033[0m");
			int count = 0;
			for (auto &sentence : sentenceList["sentences"])
			{
				if (count < maxSentence)
				{
					count++;
					printf("        %d. %s\n", count, sentence["eng"].get<string>().c_str());
					printf("           %s\n", sentence["chn"].get<string>().c_str());
				}
				else
					break;
			}
		}
	}
}
int main(int argc, char *argv[])
{
	char const *host = "www.ireading.site";
	char const *message_fmt = "GET /word/detail/?json=true&tag=false&word=%s HTTP/1.0\r\n\r\n";
	char message[1024];
	bool isBrief = true;

	if (argc < 2)
	{
		puts("Parameter: <word>");
		exit(0);
	}
	string cat_input{ "" };
	bool isPreviousDetail = false;
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--detail"))
		{ //argv[i] == "-d"|"--detail"
			isBrief = false;
			isPreviousDetail = true;
		}
		else if (isPreviousDetail) {
			isPreviousDetail = false;
			maxSentence = atoi(argv[i]);
		}
		else
		{ //argv[i] != "-d"|"--detail"
			if (i != 1)
			{
				cat_input += "%20";
			}
			cat_input += argv[i];
		}
	}
	sprintf(message, message_fmt, cat_input.c_str());

	string response = httpsRuest(host, message);
	try {
		response = response.substr(response.find('{'));
		if (isBrief)
		{ //brief
			parseBrief(json::parse(response)["wordBrief"]);
		}
		else
		{
			parseDetail(json::parse(response));
		}
	}
	catch (std::out_of_range) {
		printf("\033[0m");
		printf("该单词不存在\n");
	}

	printf("\033[0m");

	return 0;
}