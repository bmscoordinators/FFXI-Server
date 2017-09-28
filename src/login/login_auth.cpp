/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

  This file is part of DarkStar-server source code.

===========================================================================
*/

#include "../common/showmsg.h"
#include "../common/socket.h"

#include "account.h"
#include "login.h"
#include "login_auth.h"
#include "message_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <sstream> 

#include <algorithm>
#include <curl/curl.h>
#include <thread>

char randChar() {
    static const std::string capNum = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return capNum[rand() % (capNum.size()-1)]; 
}

std::string randStr(size_t length)
{
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randChar);
    return str;
}

void send_email(std::string sender, std::string receiver, std::string subject, std::string content)
{
    /**
        JP Park 9/21/2017
        Send email using sendgrid v3 api
    */

    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    std::ifstream apiKey("conf/sendgrid.conf");
    std::string  apiKeyString;
    std::getline(apiKey, apiKeyString);
    std::string authorizationHeader = "Authorization:  Bearer ";
    authorizationHeader.append(apiKeyString);

    headers = curl_slist_append(headers, authorizationHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.sendgrid.com/v3/mail/send");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        std::string jsonObj =  "{\"personalizations\":[{\"to\":[{\"email\":\""
            +receiver+"\"}],\"subject\":\""+subject+"\"}],\"from\":{\"email\":\""
            +sender+"\"},\"content\":[{\"type\":\"text/plain\",\"value\":\""+content+"\"}]}";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonObj.c_str());

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
        { 
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }
        /* always cleanup */
       curl_easy_cleanup(curl);
    }
}
void send_signup_verification_email(std::string receiver, std::string token)
{
    /*
    JP Park 9/21/2017
    Send email using sendgrid v3 api, this call may take time so we want to call it asynchronously.
    */
    std::thread(send_email, "bmsffxi@gmail.com", receiver, "BMS FFXI Signup Verification Code",
        "Please use this verification code to signup (Capital Letters and numbers only): " + token).detach();
}
std::string getUntilFirstPlusSign(std::string netId)
{
    /**
        example inputs: ab1234+123, ab1234+ase12, ab1234+AS2s1, ab1234+d+12+, ab1234++++32
        example output: ab1234
    */
    auto found = netId.find('+');
    if (found != std::string::npos)
    {
        //found, truncate
        netId = netId.substr(0,found); 
    }
    return netId;
}
std::string toLower(std::string netId)
{
    std::transform(netId.begin(), netId.end(), netId.begin(), ::tolower);
    return netId;
}
std::string unifyNetId(std::string netId)
{
    /*
    JP Park 9/26/2017
    if you send email to aB1234+randomstring@nyu.edu, then it will arrive at ab1234@nyu.edu
    So it is necessary for any given email, generate one unique representation for our use.
    This can be done by making all the capital letters to lower letters and then removing everything after +
    */
    netId = getUntilFirstPlusSign(netId);
    netId = toLower(netId);
    return netId;
}

int32 login_fd;					//main fd(socket) of server

/*
*
*		LOGIN SECTION
*
*/
int32 connect_client_login(int32 listenfd)
{
    int32 fd = 0;
    struct sockaddr_in client_address;
    if ((fd = connect_client(listenfd, client_address)) != -1)
    {
        int32 code = create_session(fd, recv_to_fifo, send_from_fifo, login_parse);
        session[fd]->client_addr = ntohl(client_address.sin_addr.s_addr);
        return fd;
    }
    return -1;
}

int32 login_parse(int32 fd)
{
    login_session_data_t* sd = (login_session_data_t*)session[fd]->session_data;

    //check if sd will not defined
    if (sd == nullptr)
    {
        session[fd]->session_data = new login_session_data_t{};
        sd = (login_session_data_t*)session[fd]->session_data;
        sd->serviced = 0;
        login_sd_list.push_back(sd);
        sd->client_addr = session[fd]->client_addr;
        sd->login_fd = fd;
    }

    if (session[fd]->flag.eof)
    {
        do_close_login(sd, fd);
        return 0;
    }

    //all auth packets have one structure:
    // [login][passwords][code] => summary assign 33 bytes
    /**
      JP: there is reason why code here use name.c_str() instead of name
       - name is 16 character string which might contain garbage.
       - name.c_str() truncates until only valid string.
    */
    if (session[fd]->rdata.size() == 33 )
    {
        char* buff = &session[fd]->rdata[0];
        int8 code = RBUFB(buff, 32);

        std::string name(buff, buff + 16);
        std::string password(buff + 16, buff + 32);
        std::string NYUNetID;  
        std::string securityCode;
        std::string uniqueNetId;
        uint8 isNextRow, signedUp, char_auth_id;
        std::string serverSideVerificationCode;
        uint32 accid;
        char anotherPacket[33] = { '0' };
        int len;

        std::fill_n(sd->login, sizeof sd->login, '\0');
        std::copy(name.cbegin(), name.cend(), sd->login); 

        //data check
        if (check_string(name, 16) && check_string(password, 16))
        {
            ShowWarning(CL_WHITE"login_parse" CL_RESET":" CL_WHITE"%s" CL_RESET" send unreadable data\n", ip2str(sd->client_addr, nullptr));
            session[fd]->wdata.resize(1);
            WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR;
            do_close_login(sd, fd);
            return -1;
        }

        switch (code)
        {
        case LOGIN_ATTEMPT:
        {
            const int8* fmtQuery = "SELECT accounts.id,accounts.status \
									FROM accounts \
									WHERE accounts.login = '%s' AND accounts.password = PASSWORD('%s')";
            int32 ret = Sql_Query(SqlHandle, fmtQuery, name.c_str(), password.c_str());
            if (ret != SQL_ERROR  && Sql_NumRows(SqlHandle) != 0)
            {
                ret = Sql_NextRow(SqlHandle);

                sd->accid = (uint32)Sql_GetUIntData(SqlHandle, 0);
                uint8 status = (uint8)Sql_GetUIntData(SqlHandle, 1);

                if (status & ACCST_NORMAL)
                {
                    //fmtQuery = "SELECT * FROM accounts_sessions WHERE accid = %d AND client_port <> 0";

                    //int32 ret = Sql_Query(SqlHandle,fmtQuery,sd->accid);

                    //if( ret != SQL_ERROR && Sql_NumRows(SqlHandle) != 0 )
                    //{
                    //	WBUFB(session[fd]->wdata,0) = 0x05; // SESSION has already activated
                    //	WFIFOSET(fd,33);
                    //	do_close_login(sd,fd);
                    //	return 0;
                    //}
                    fmtQuery = "UPDATE accounts SET accounts.timelastmodify = NULL WHERE accounts.id = %d";
                    Sql_Query(SqlHandle, fmtQuery, sd->accid);
                    fmtQuery = "SELECT charid, server_addr, server_port \
                                FROM accounts_sessions JOIN accounts \
                                ON accounts_sessions.accid = accounts.id \
                                WHERE accounts.id = %d;";
                    ret = Sql_Query(SqlHandle, fmtQuery, sd->accid);
                    if (ret != SQL_ERROR  && Sql_NumRows(SqlHandle) == 1)
                    {
                        while (Sql_NextRow(SqlHandle) == SQL_SUCCESS)
                        {
                            uint32 charid = Sql_GetUIntData(SqlHandle, 0);
                            uint64 ip = Sql_GetUIntData(SqlHandle, 1);
                            uint64 port = Sql_GetUIntData(SqlHandle, 2);

                            ip |= (port << 32);

                            zmq::message_t chardata(sizeof(charid));
                            WBUFL(chardata.data(), 0) = charid;
                            zmq::message_t empty(0);

                            queue_message(ip, MSG_LOGIN, &chardata, &empty);
                        }
                    }
                    memset(&session[fd]->wdata[0], 0, 33);
                    session[fd]->wdata.resize(33);
                    WBUFB(session[fd]->wdata.data(), 0) = LOGIN_SUCCESS;
                    WBUFL(session[fd]->wdata.data(), 1) = sd->accid;
                    flush_fifo(fd);
                    do_close_tcp(fd);
                }
                else if (status & ACCST_BANNED)
                {
                    memset(&session[fd]->wdata[0], 0, 33);
                    session[fd]->wdata.resize(33);
                    //	WBUFB(session[fd]->wdata,0) = LOGIN_SUCCESS;
                    do_close_login(sd, fd);
                }

                //////22/03/2012 Fix for when a client crashes before fully logging in:
                //				Before: When retry to login, would freeze client since login data corrupt.
                //				After: Removes older login info if a client logs in twice (based on acc id!)

                //check for multiple logins from this account id
                int numCons = 0;
                for (login_sd_list_t::iterator i = login_sd_list.begin(); i != login_sd_list.end(); ++i) {
                    if ((*i)->accid == sd->accid) {
                        numCons++;
                    }
                }

                if (numCons > 1) {
                    ShowInfo("login_parse:" CL_WHITE"<%s>" CL_RESET" has logged in %i times! Removing older logins.\n", name.c_str(), numCons);
                    for (int j = 0; j < (numCons - 1); j++) {
                        for (login_sd_list_t::iterator i = login_sd_list.begin(); i != login_sd_list.end(); ++i) {
                            if ((*i)->accid == sd->accid) {
                                //ShowInfo("Current login fd=%i Removing fd=%i \n",sd->login_fd,(*i)->login_fd);
                                login_sd_list.erase(i);
                                break;
                            }
                        }
                    }
                }
                //////

                ShowInfo("login_parse:" CL_WHITE"<%s>" CL_RESET" was connected\n", name.c_str(), status);
                return 0;
            }
            else {
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR;
                ShowWarning("login_parse: unexisting user" CL_WHITE"<%s>" CL_RESET" tried to connect\n", name.c_str());
                do_close_login(sd, fd);
            }
        }
        break;
        /**
            LOGIN_CREATE is now deprecated.
            The login programs who want to sign up will be using this code, and we do not allow them to sign up.
            - JP. 9/24/17
        */
        case LOGIN_CREATE: 
            session[fd]->wdata.resize(1);
            WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
            do_close_login(sd, fd);
            return -1;
            break;
        /* New LOGIN_CREATE using NYUNetID and verification code -JP. 9/24/17 */
        case LOGIN_CREATE_NYU:  
            // Receive another packet for NYUNetID and verification code.
            len = sRecv(fd, (char *)anotherPacket, 32, 0);
            if (len == SOCKET_ERROR)
            {//An exception has occured
                if (sErrno != S_EWOULDBLOCK) {
                    set_eof(fd);
                }
                return 0;
            }
            if (len == 0)
            {//Normal connection end.
                set_eof(fd);
                return 0;
            }

            NYUNetID = std::string(anotherPacket, anotherPacket + 16);
            securityCode = std::string(anotherPacket + 16, anotherPacket + 32);

            // there are many different forms of NetID for one NetID. Just use the simplest form in order to keep track.
            uniqueNetId = unifyNetId(NYUNetID.c_str());

            // Check if char_auth table exists.
            if (Sql_Query(SqlHandle, "SELECT 1 FROM `char_auth` LIMIT 1;") == SQL_ERROR)
            {
                // char_auth table does not exist; which means there is no verification code to match.
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_ERROR_MATCHING;
                do_close_login(sd, fd);
                return -1;
            }

            /**
                Check if there is corresponding game id for this NYUNetID.
                I am using SHA1 function in mysql query in order to hash NYUNetID.
                We are not storing the original - JP, 9/27/17
            */
            if (Sql_Query(SqlHandle, "SELECT account_generated FROM char_auth WHERE char_auth.netid = SHA1('%s')", uniqueNetId) == SQL_ERROR)
            {
                ShowWarning(CL_WHITE"login_parse" CL_RESET": char_auth NetID SELECT<" CL_WHITE"%s" CL_RESET"> Failed\n", uniqueNetId);
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                do_close_login(sd, fd);
                return -1;
            }
            // No verification code has been sent to this NYUNetID 
            if (Sql_NumRows(SqlHandle) == 0)
            {
                // No verification code to match, this is an error.
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_ERROR_MATCHING; 
                do_close_login(sd, fd);
                return -1;
            }
            // Verification code has been sent to this NYUNetID.
            else
            {
                isNextRow = Sql_NextRow(SqlHandle);
                signedUp = (uint8)Sql_GetUIntData(SqlHandle, 0);
                if (signedUp)
                {
                    // Game ID for this NYU ID already exists.
                    session[fd]->wdata.resize(1);
                    WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
                    do_close_login(sd, fd);
                    return -1;
                }
                else
                {
                    // Create Game ID
                    if (Sql_Query(SqlHandle, "SELECT id FROM char_auth WHERE char_auth.netid = SHA1('%s') and char_auth.auth_token = SHA1('%s')", uniqueNetId, securityCode.c_str()) == SQL_ERROR)
                    {
                        session[fd]->wdata.resize(1);
                        WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                        do_close_login(sd, fd);
                        return -1;
                    }

                    if (Sql_NumRows(SqlHandle) == 0)
                    {
                        // no match found
                        session[fd]->wdata.resize(1);
                        WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_ERROR_MATCHING;
                        do_close_login(sd, fd);
                        return -1;
                    }
                    else
                    {
                        isNextRow = Sql_NextRow(SqlHandle);
                        char_auth_id = (uint8)Sql_GetUIntData(SqlHandle, 0); //well, we can select using token and netid, but...
                        // create game account
                        //looking for same login
                        if (Sql_Query(SqlHandle, "SELECT accounts.id FROM accounts WHERE accounts.login = '%s'", name.c_str()) == SQL_ERROR)
                        {
                            session[fd]->wdata.resize(1);
                            WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
                            do_close_login(sd, fd);
                            return -1;
                        }

                        if (Sql_NumRows(SqlHandle) == 0)
                        {
                            //creating new account_id
                            char *fmtQuery = "SELECT max(accounts.id) FROM accounts;";

                            accid = 0;

                            if (Sql_Query(SqlHandle, fmtQuery) != SQL_ERROR  && Sql_NumRows(SqlHandle) != 0)
                            {
                                Sql_NextRow(SqlHandle);

                                accid = Sql_GetUIntData(SqlHandle, 0) + 1;
                            }
                            else {
                                session[fd]->wdata.resize(1);
                                WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
                                do_close_login(sd, fd);
                                return -1;
                            }

                            accid = (accid < 1000 ? 1000 : accid);

                            //creating new account
                            time_t timecreate;
                            tm*	   timecreateinfo;

                            time(&timecreate);
                            timecreateinfo = localtime(&timecreate);

                            char strtimecreate[128];
                            strftime(strtimecreate, sizeof(strtimecreate), "%Y:%m:%d %H:%M:%S", timecreateinfo);
                            fmtQuery = "INSERT INTO accounts(id,login,password,timecreate,timelastmodify,status,priv,content_ids,email)\
									   VALUES(%d,'%s',PASSWORD('%s'),'%s',NULL,%d,%d,%d,'%s');";

                            if (Sql_Query(SqlHandle, fmtQuery, accid, name.c_str(), password.c_str(),
                                strtimecreate, ACCST_NORMAL, ACCPRIV_USER, 1, NYUNetID.c_str()) == SQL_ERROR)
                            {
                                session[fd]->wdata.resize(1);
                                WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
                                do_close_login(sd, fd);
                                return -1;
                            }

                            ShowStatus(CL_WHITE"login_parse" CL_RESET": account<" CL_WHITE"%s" CL_RESET"> was created\n", name.c_str());
                            session[fd]->wdata.resize(1);
                            WBUFB(session[fd]->wdata.data(), 0) = LOGIN_SUCCESS_CREATE;
                            do_close_login(sd, fd);

                            // Update char_auth with newly created game account information! (IMPORTANT)
                            if (Sql_Query(SqlHandle, "UPDATE char_auth SET account_generated=1, account_id=%d, auth_token='' WHERE id = %d", accid, char_auth_id) == SQL_ERROR)
                            {
                                session[fd]->wdata.resize(1);
                                WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                                do_close_login(sd, fd);
                                return -1;
                            }
                        }
                        else {
                            // the same game id exists;
                            ShowWarning(CL_WHITE"login_parse" CL_RESET": account<" CL_WHITE"%s" CL_RESET"> already exists\n", name.c_str());
                            session[fd]->wdata.resize(1);
                            WBUFB(session[fd]->wdata.data(), 0) = LOGIN_ERROR_CREATE;
                            do_close_login(sd, fd);
                        }
                        
                    }
                }
            }
            break;
        case SEND_VERIFICATION:
            /**
                JP: if same nyuid is not in char_auth table, send an email with code! and record it in db
                ASSUMING SQL ATTACKS ARE PREVENTED BY SQL_QUERY FUNCTION (PREPARE)
                Necessary to use some fill value for password from XiLoader.
            */
            uniqueNetId = unifyNetId(name.c_str()); 
            const int8* fmtQuery;

            // Create Table if not exist
            if (Sql_Query(SqlHandle, "CREATE TABLE IF NOT EXISTS `char_auth` ( "
                "`id` int(10) unsigned NOT NULL AUTO_INCREMENT, "
                "`netid` varchar(64) NOT NULL UNIQUE, " 
                "`auth_token` varchar(64) NOT NULL DEFAULT '', "
                "`timecreate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00', "
                "`email_sent` tinyint(1) unsigned NOT NULL DEFAULT '0', "
                "`account_generated` tinyint(1) unsigned NOT NULL DEFAULT '0', "
                "`account_id` int(10) unsigned NOT NULL DEFAULT '0', "
                "PRIMARY KEY (`id`) ) ENGINE=MyISAM DEFAULT CHARSET=utf8;") == SQL_ERROR)
            {
                ShowWarning("char_auth table not created\n");
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                do_close_login(sd, fd);
                return -1;
            }

            //adding new entry in char_auth for this NYUNetID
            time_t timecreate;
            tm*	   timecreateinfo;

            time(&timecreate);
            timecreateinfo = localtime(&timecreate);

            char currentStringTime[128];
            strftime(currentStringTime, sizeof(currentStringTime), "%Y:%m:%d %H:%M:%S", timecreateinfo);

            if (Sql_Query(SqlHandle, "SELECT account_generated FROM char_auth WHERE char_auth.netid = SHA1('%s')", uniqueNetId) == SQL_ERROR)
            {
                ShowWarning(CL_WHITE"login_parse" CL_RESET": char_auth NetID SELECT<" CL_WHITE"%s" CL_RESET"> Failed\n", uniqueNetId);
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                do_close_login(sd, fd);
                return -1;
            }
            if (Sql_NumRows(SqlHandle) == 0) 
            {
                std::string token = randStr(6);
                fmtQuery = "INSERT INTO char_auth(netid,auth_token,timecreate,email_sent)\
									   VALUES(SHA1('%s'),SHA1('%s'),'%s',%d);";
                if (Sql_Query(SqlHandle, fmtQuery, uniqueNetId.c_str(), token.c_str(), currentStringTime, true) == SQL_ERROR)  // make sure this is the same as prepare
                {
                    session[fd]->wdata.resize(1);
                    WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                    do_close_login(sd, fd);
                    return -1;
                }
                send_signup_verification_email(uniqueNetId +"@nyu.edu", token);
                session[fd]->wdata.resize(1);
                WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_SUCCESS_SENDING;
                do_close_login(sd, fd); 
                return -1; 
            }
            else {
                /**
                    1) user has game account - STOP, give error code
                    2) user has not signed up - Send another email, ignore first one
                */
                uint8 ret = Sql_NextRow(SqlHandle);  // wasn't i supposed to declare this variable at the beginning?
                uint8 signedUp = (uint8)Sql_GetUIntData(SqlHandle, 0);
                if (signedUp)
                {
                    ShowWarning(CL_WHITE"login_parse" CL_RESET": NYU NetID<" CL_WHITE"%s" CL_RESET"> already exists\n", uniqueNetId);
                    session[fd]->wdata.resize(1);
                    WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_ERROR_SENDING;
                    do_close_login(sd, fd);
                    return -1;
                }
                else
                {
                    std::string token = randStr(6);
                    fmtQuery = "UPDATE char_auth SET auth_token = SHA1('%s') WHERE netid = SHA1('%s')";
                    if (Sql_Query(SqlHandle, fmtQuery, token, uniqueNetId) == SQL_ERROR)  // make sure this is the same as prepare
                    {
                        session[fd]->wdata.resize(1);
                        WBUFB(session[fd]->wdata.data(), 0) = DB_ERROR;
                        do_close_login(sd, fd);
                        return -1;
                    }
                    send_signup_verification_email(uniqueNetId + "@nyu.edu", token);

                    session[fd]->wdata.resize(1);
                    WBUFB(session[fd]->wdata.data(), 0) = VERIFICATION_SUCCESS_SENDING;
                    do_close_login(sd, fd);
                    return -1;
                }
            }
            break;
        default:
            ShowWarning("login_parse: undefined code:[%d], ip sender:<%s>\n", code, ip2str(session[fd]->client_addr, nullptr));
            do_close_login(sd, fd);
            break;
        };
        //RFIFOSKIP(fd,33);
    }
    else {
        do_close_login(sd, fd);
    }
    return 0;
};


int32 do_close_login(login_session_data_t* loginsd, int32 fd)
{
    ShowInfo(CL_WHITE"login_parse" CL_RESET":" CL_WHITE"%s" CL_RESET"shutdown socket...\n", ip2str(loginsd->client_addr, nullptr));
    erase_loginsd(fd);
    do_close_tcp(fd);
    return 0;
}

bool check_string(std::string const& str, std::size_t max_length)
{
    return !str.empty() && str.size() <= max_length && std::all_of(str.cbegin(), str.cend(), [](char const& c) { return c >= 0x20; });
}