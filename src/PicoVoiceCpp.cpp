#include <iostream>
#include <map>
#include <sstream>
#include <cstring>

#include <ao/ao.h>

extern "C"
{
#include "svoxpico/picoapi.h"
#include "svoxpico/picoapid.h"
#include "svoxpico/picoos.h"
}

/*
================================================
Pico

class to encapsulate the workings of the SVox PicoTTS System
================================================
*/
class Pico
{
private:
    picoos_SDFile       sdOutFile;
    char *              out_filename;

    pico_Char *         local_text;

    int             m_Speed;
    int             m_Pitch;
    int             m_Volume;
    std::string     m_ResourcePath;
    std::string     m_LastError;

    void*           m_PicoMemArea;
    pico_System     m_PicoSystem;
    pico_Resource   m_PicoTaResource;
    pico_Resource   m_PicoSgResource;
    pico_Engine     m_PicoEngine;

    std::string     m_VoiceLang;
    const std::map<std::string, std::string> m_VoiceParts = {
        { "de-DE", "gl0"},
        { "en-GB", "kh0"},
        { "en-US", "lh0"},
        { "es-ES", "zl0"},
        { "fr-FR", "nk0"},
        { "it-IT", "cm0"}
    };
    const std::string     m_VoiceName = "DefaultVoice";
    pico_Char*            m_PicoVoiceName;

    ao_sample_format    m_PcmFormat;
    ao_device*          m_PcmDevice;
    int                 m_PcmDriver;

    const int       OUT_BUFFER_SIZE = 256;
    const int       PCM_BUFFER_SIZE = 512;
    const int       WRK_BUFFER_SIZE = 2500000;

    void SetPicoError(std::string text, int errorNo);

public:
    Pico() ;
    virtual ~Pico() ;

    void SetPath(const std::string& path);
    bool SetVoice(const std::string& lang);
    void SetSpeed(float val);
    void SetPitch(float val);
    void SetVolume(float val);
    std::string GetPicoError();

    bool InitializeSystem();
    bool Process(const std::string &text);
    void Cleanup() ;

    void setOutFilename( const char * fn ) { out_filename = const_cast<char*>(fn); }
};

Pico::Pico()
{
    m_PicoEngine    = nullptr;
    m_PicoTaResource= nullptr;
    m_PicoSgResource= nullptr;
    m_PicoSystem    = nullptr;
    m_PicoMemArea   = nullptr;

    sdOutFile           = 0;
    out_filename        = 0;

    m_PcmFormat.bits        = 2 * 8;
    m_PcmFormat.channels    = 1;
    m_PcmFormat.rate        = 16000;
    m_PcmFormat.byte_format = AO_FMT_LITTLE;
    m_PcmFormat.matrix      = nullptr;
    m_PcmDevice = nullptr;
    m_PcmDriver = 0;

    m_Speed = 88;
    m_Pitch = 105;
    m_Volume= 100;
    m_ResourcePath  = "./lang/";
    m_VoiceLang     = "FR-fr";

    m_PicoVoiceName = (pico_Char*) m_VoiceName.c_str();
}

Pico::~Pico()
{
    Cleanup();
}

void Pico::SetPath(const std::string& path)
{
    m_ResourcePath = path;

    if((m_ResourcePath.back()!='/')&&((m_ResourcePath.back()!='\\')))
        m_ResourcePath.push_back('/');
}

void Pico::SetSpeed(float val)
{
    m_Speed = val*100;
}

void Pico::SetPitch(float val)
{
    m_Pitch = val*100;
}

void Pico::SetVolume(float val)
{
    m_Volume = val*100;
}

bool Pico::SetVoice(const std::string& lang)
{
    std::map<std::string, std::string>::const_iterator it = m_VoiceParts.find(lang);
    if(it==m_VoiceParts.end())
    {
        SetPicoError("Unknown voice language", 0);
        return false;
    }

    m_VoiceLang = lang;
    return true;
}

bool Pico::InitializeSystem()
{
    int             ret;
    std::string     fileName;
    pico_Char       resName[PICO_MAX_RESOURCE_NAME_SIZE];


    m_PicoMemArea = malloc(WRK_BUFFER_SIZE);
    ret = pico_initialize(m_PicoMemArea, WRK_BUFFER_SIZE, &m_PicoSystem);
    if(ret)
    {
        SetPicoError("Cannot initialize pico", ret);
        Cleanup();
        return false;
    }

    //*** Load the text analysis resource file
    fileName = m_ResourcePath + m_VoiceLang + "_ta.bin";
    ret = pico_loadResource(m_PicoSystem, (pico_Char *) fileName.c_str(), &m_PicoTaResource);
    if(ret)
    {
        SetPicoError("Cannot load text analysis resource file", ret);
        Cleanup();
        return false;
    }

    //*** Load the signal generation resource file
    fileName = m_ResourcePath + m_VoiceLang +"_"+ m_VoiceParts.at(m_VoiceLang) + "_sg.bin";
    ret = pico_loadResource(m_PicoSystem, (pico_Char *) fileName.c_str(), &m_PicoSgResource);
    if(ret)
    {
        SetPicoError("Cannot load signal generation resource file", ret);
        Cleanup();
        return false;
    }

    //*** Create a voice definition
    ret = pico_createVoiceDefinition(m_PicoSystem, m_PicoVoiceName);
    if(ret)
    {
        SetPicoError("Cannot create voice definition", ret);
        Cleanup();
        return false;
    }

    //*** Get the text analysis resource name
    ret = pico_getResourceName(m_PicoSystem, m_PicoTaResource, (char*) resName);
    if(ret)
    {
        SetPicoError("Cannot get the text analysis resource name", ret);
        Cleanup();
        return false;
    }

    //*** Add the text analysis resource to the voice
    ret = pico_addResourceToVoiceDefinition(m_PicoSystem, m_PicoVoiceName, resName);
    if(ret)
    {
        SetPicoError("Cannot add the text analysis resource to the voice", ret);
        Cleanup();
        return false;
    }

    //*** Get the signal generation resource name
    ret = pico_getResourceName(m_PicoSystem, m_PicoSgResource, (char*) resName);
    if(ret)
    {
        SetPicoError("Cannot get the signal generation resource name", ret);
        Cleanup();
        return false;
    }

    //*** Add the signal generation resource to the voice
    ret = pico_addResourceToVoiceDefinition(m_PicoSystem, m_PicoVoiceName, resName);
    if(ret)
    {
        SetPicoError("Cannot add the signal generation resource to the voice", ret);
        Cleanup();
        return false;
    }

    //*** Create a new Pico engine
    ret = pico_newEngine(m_PicoSystem, m_PicoVoiceName, &m_PicoEngine);
    if(ret)
    {
        SetPicoError("Cannot create a new pico engine", ret);
        Cleanup();
        return false;
    }

    //*** Initialize libao, find and open default audio driver
    ao_initialize();
    m_PcmDriver = ao_default_driver_id();
    m_PcmDevice = ao_open_live(m_PcmDriver, &m_PcmFormat, 0);
    if(m_PcmDevice == 0)
    {
        SetPicoError("Error opening audio device", 0);
        Cleanup();
        return false;
    }

    //*** Success
    return true;
}

void Pico::Cleanup()
{
    if(m_PicoEngine != nullptr)
    {
        pico_disposeEngine( m_PicoSystem, &m_PicoEngine );
        pico_releaseVoiceDefinition( m_PicoSystem, m_PicoVoiceName );
        m_PicoEngine = nullptr;
    }

    if(m_PicoSgResource != nullptr)
    {
        pico_unloadResource( m_PicoSystem, &m_PicoSgResource );
        m_PicoSgResource = nullptr;
    }

    if(m_PicoTaResource != nullptr)
    {
        pico_unloadResource( m_PicoSystem, &m_PicoTaResource );
        m_PicoTaResource = nullptr;
    }

    if(m_PicoSystem != nullptr)
    {
        pico_terminate(&m_PicoSystem);
        m_PicoSystem = nullptr;
    }

    if(m_PicoMemArea != nullptr)
    {
        free(m_PicoMemArea);
        m_PicoMemArea = nullptr;
    }

    if(m_PcmDevice != nullptr)
    {
        ao_close(m_PcmDevice);
        ao_shutdown();
        m_PcmDevice = nullptr;
    }
}

bool Pico::Process(const std::string &text)
{
    short   outBuffer[OUT_BUFFER_SIZE/2];
    char    pcmBuffer[PCM_BUFFER_SIZE];
    int     bufferUsed = 0;
    int     ret, status;

    std::ostringstream oss;
    std::string     localString;
    long long int   textLength;
    pico_Int16      computeLength = 0;
    pico_Int16      bytesSent;
    pico_Int16      bytesRecv;
    pico_Int16      dataType;
    pico_Char*      picoPtr = nullptr;


    //*** Add optionnal speed/pitch/volume
    if((m_Speed!=100)||(m_Pitch!=100)||(m_Volume!=100))
    {
        oss << "<speed level=\"" << m_Speed << "\">";
        oss << "<pitch level=\"" << m_Pitch<< "\">";
        oss << "<volume level=\"" << m_Volume <<"\">";
        oss << text;
        oss << "</speed></pitch></volume>";
        localString = oss.str();
        textLength = localString.size()+1;
        picoPtr = (pico_Char*) localString.c_str();
    }
    else
    {
        textLength = text.size()+1;
        picoPtr = (pico_Char*) text.c_str();
    }

    //*** Synthesis loop
    while(1)
    {
        if(computeLength <= 0)
        {
            if(textLength <= 0)
                break;

            ret = textLength >= 32767 ? 32767 : textLength;
            textLength -= ret;
            computeLength = ret;
        }

        //*** Feed the text into the engine
        ret = pico_putTextUtf8(m_PicoEngine, picoPtr, computeLength, &bytesSent);
        if(ret)
        {
            SetPicoError("Cannot put text", ret);
            return false;
        }

        computeLength -= bytesSent;
        picoPtr += bytesSent;

        do
        {
            //*** Retrieve the samples
            status = pico_getData(m_PicoEngine, (void *) outBuffer, OUT_BUFFER_SIZE, &bytesRecv, &dataType);
            if((status !=PICO_STEP_BUSY) && (status !=PICO_STEP_IDLE))
            {
                SetPicoError("Cannot get data", status);
                return false;
            }

            //*** Copy partial encoding
            if(bytesRecv > 0)
            {
                //*** In stand by buffer
                if ( (bufferUsed + bytesRecv) <= PCM_BUFFER_SIZE )
                {
                    memcpy( pcmBuffer+bufferUsed, (int8_t *)outBuffer, bytesRecv );
                    bufferUsed += bytesRecv;
                }
                //*** Play with libao
                else
                {
                    ao_play(m_PcmDevice, pcmBuffer, bufferUsed);
                    bufferUsed = 0;
                    memcpy( pcmBuffer, (int8_t *)outBuffer, bytesRecv );
                    bufferUsed += bytesRecv;
                }
            }

        } while (PICO_STEP_BUSY == status);

        //*** Play end of buffer
        ao_play(m_PcmDevice, pcmBuffer, bufferUsed);
    }

    return true;
}

void Pico::SetPicoError(std::string text, int errorNo)
{
    if(errorNo != 0)
    {
        pico_Retstring  outMessage;
        std::ostringstream oss;

        pico_getSystemStatusMessage(m_PicoSystem, errorNo, outMessage);
        oss << text << "(error code " << errorNo << ") : " << outMessage;
        m_LastError = oss.str();
    }
    else
    {
        m_LastError = text;
    }
}

std::string Pico::GetPicoError()
{
    return m_LastError;
}

int main( int argc, const char ** argv )
{
    Pico pico;

    pico.SetPath("./lang");
    pico.setOutFilename("pico.wav");
    if(!pico.SetVoice("fr-FR"))
    {
        std::cout << pico.GetPicoError() << std::endl;
        return -1;
    }

    if(!pico.InitializeSystem())
    {
        std::cout << pico.GetPicoError() << std::endl;
        return -2;
    }

    pico.SetSpeed(1.05);
    pico.SetPitch(0.55);
    pico.Process("Bonjour, je m'appelle François, et toi ?");

    pico.SetSpeed(1.1);
    pico.SetPitch(1.05);
    pico.Process("Moi, je m'appelle Cécile. Il va neiger aujourd'hui.");

    pico.SetSpeed(1.05);
    pico.SetPitch(0.55);
    pico.Process("Super ! On va faire du ski ?");

    pico.SetSpeed(1.1);
    pico.SetPitch(1.05);
    pico.Process("Ah non ! La neige c'est froid et ça mouille.");

    pico.Process("Au Japon, un trésor national est un bien culturel d'une valeur exceptionnelle, et de portée universelle, appartenant au patrimoine culturel de la nation. Depuis 1950, le ministère de l'Éducation, de la Culture, des Sports, des Sciences et de la Technologie dispose de l'arsenal législatif nécessaire pour inventorier et classer les biens culturels du pays, et mettre en œuvre les mesures appropriées pour leur protection et conservation.");

    pico.Cleanup();
}

