#include "VPXBatoceraCaptureTool.h"

#include <plog/Log.h>

#include "inc/mongoose/mongoose.h"
#include "inc/mINI/ini.h"
#include "inc/subprocess/subprocess.h"
#include "inc/lodepng/lodepng.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

#include <libimagequant.h>

static VPXBatoceraCaptureTool* g_pServer = NULL;

void VPXBatoceraCaptureTool::Forward(struct mg_http_message *hm, struct mg_connection *c)
{
   size_t i, max = sizeof(hm->headers) / sizeof(hm->headers[0]);
   struct mg_str host = mg_url_host(g_pServer->m_szESUrl.c_str());

   mg_printf(c, "%.*s ", (int) (hm->method.len), hm->method.buf);

   if (hm->uri.len > 0)
      mg_printf(c, "%.*s ", (int) (hm->uri.len - 3), hm->uri.buf + 3);

   mg_printf(c, "%.*s\r\n", (int) (hm->proto.len), hm->proto.buf);

   for (i = 0; i < max && hm->headers[i].name.len > 0; i++) {
     struct mg_str *k = &hm->headers[i].name, *v = &hm->headers[i].value;
     if (mg_strcmp(*k, mg_str("Host")) == 0)
        v = &host;

     mg_printf(c, "%.*s: %.*s\r\n", (int) k->len, k->buf, (int) v->len, v->buf);
   }
   mg_send(c, "\r\n", 2);
   mg_send(c, hm->body.buf, hm->body.len);
}

void VPXBatoceraCaptureTool::HandleEvent2(struct mg_connection *c, int ev, void *ev_data)
{
  struct mg_connection *c2 = (struct mg_connection *) c->fn_data;
  if (ev == MG_EV_READ) {
    if (c2 != NULL)
       mg_send(c2, c->recv.buf, c->recv.len);
    mg_iobuf_del(&c->recv, 0, c->recv.len);
  }
  else if (ev == MG_EV_CLOSE) {
    if (c2 != NULL)
       c2->fn_data = NULL;
  }
  (void) ev_data;
}

void VPXBatoceraCaptureTool::HandleEvent(struct mg_connection *c, int ev, void *ev_data)
{
  struct mg_connection *c2 = (struct mg_connection *)c->fn_data;
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (!strncmp(hm->uri.buf, "/capture?", strlen("/capture?"))) {
       PLOGI.printf("Capture request: uri=%.*s", hm->uri.len, hm->uri.buf);
       g_pServer->Capture(c, ev_data);
    }
    else if (!strncmp(hm->uri.buf, "/generate-description?", strlen("/generate-description?"))) {
       PLOGI.printf("Generate description request: uri=%.*s", hm->uri.len, hm->uri.buf);
       g_pServer->GenerateDescription(c, ev_data);
    }
    else if (!strncmp(hm->uri.buf, "/es/", strlen("/es/"))) {
       PLOGI.printf("ES forward request: uri=%.*s", hm->uri.len - 3, hm->uri.buf + 3);
       c2 = mg_connect(c->mgr, g_pServer->m_szESUrl.c_str(), VPXBatoceraCaptureTool::HandleEvent2, c);
       if (c2) {
          c->fn_data = c2;
          Forward(hm, c2);
          c->is_resp = 0;
          c2->is_hexdumping = 0;
       }
       else {
          PLOGI.printf("Unable to forward");
          mg_error(c, "Cannot create backend connection");
       }
    }
    else {
       PLOGI.printf("Serving website request");
       string szHtmlFile = g_pServer->m_szBasePath + "assets/vpxbct.html";
       struct mg_http_serve_opts opts = {};
       mg_http_serve_file(c, hm, szHtmlFile.c_str(), &opts);
    }

    return;
  }
  else if (ev == MG_EV_CLOSE) {
    if (c2 != NULL) {
       PLOGI.printf("Connection close event");
       c2->is_closing = 1;
       c2->fn_data = NULL;
    }
  }
}

VPXBatoceraCaptureTool::VPXBatoceraCaptureTool()
{
}

VPXBatoceraCaptureTool::~VPXBatoceraCaptureTool()
{
}

void VPXBatoceraCaptureTool::Capture(struct mg_connection *c, void *ev_data)
{
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   char type[128];
   mg_http_get_var(&hm->query, "type", type, sizeof(type));

   if (*type == '\0') {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   ScreenSize tableSize;
   if (!GetScreenResolution(1, &tableSize)) {
      mg_http_reply(c, 500, "", "Server error");
      return;
   }

   if (!strcmp(type, "image")) {
      string szPath = m_szBasePath + "vpxbct-tmp-image.png";

      // ffmpeg -f x11grab -video_size 1080x1920 -i :0.0 -vframes 1 "vpxbct-tmp-image.png"

      string szSize = std::to_string(tableSize.width) + "x" + std::to_string(tableSize.height);
      const char* command_line[] = {"/usr/bin/ffmpeg",
                                    "-y",
                                    "-f", "x11grab",
                                    "-video_size", szSize.c_str(),
                                    "-i", ":0.0",
                                    "-vframes", "1",
                                    szPath.c_str(),
                                    NULL};
      const char* environment[] = {"DISPLAY=:0.0",  NULL};

      struct subprocess_s subprocess;
      if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
         int process_return;
         if (!subprocess_join(&subprocess, &process_return)) {
            PLOGI.printf("process returned: %d", process_return);
            if (!process_return && ProcessPNG(szPath)) {
               struct mg_http_serve_opts opts = {};
               mg_http_serve_file(c, hm, szPath.c_str(), &opts);
               return;
            }
         }
      }

      mg_http_reply(c, 500, "", "Server error");
   }
   else if (!strcmp(type, "video")) {
      string szPath = m_szBasePath + "vpxbct-tmp-video.mp4";

      // ffmpeg -f x11grab -video_size 1080x1920 -framerate 30 -i :0.0 -c:v libx264 -preset ultrafast -profile:v main -level 3.1 -pix_fmt yuv420p -t 10 "vpxbct-tmp-video.mp4"

      string szSize = std::to_string(tableSize.width) + "x" + std::to_string(tableSize.height);
      const char* command_line[] = {"/usr/bin/ffmpeg",
                                    "-y",
                                    "-f", "x11grab",
                                    "-video_size", szSize.c_str(),
                                    "-framerate", "30",
                                    "-i", ":0.0",
                                    "-c:v", "libx264",
                                    "-preset", "slow",
                                    "-profile:v", "main",
                                    "-level", "3.1",
                                    "-pix_fmt", "yuv420p",
                                    "-t", "5",
                                    szPath.c_str(),
                                    NULL};
      const char* environment[] = {"DISPLAY=:0.0",  NULL};

      struct subprocess_s subprocess;
      if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
         int process_return;
         if (!subprocess_join(&subprocess, &process_return)) {
            PLOGI.printf("process returned: %d", process_return);
            if (!process_return) {
               struct mg_http_serve_opts opts = {};
               mg_http_serve_file(c, hm, szPath.c_str(), &opts);
               return;
            }
         }
      }

      mg_http_reply(c, 500, "", "Server error");
   }
   else if (!strcmp(type, "boxart")) {
      ScreenSize backglassSize;
      if (!GetScreenResolution(2, &backglassSize)) {
         mg_http_reply(c, 500, "", "Server error");
         return;
      }

      string szPath = m_szBasePath + "vpxbct-tmp-boxart.png";

      // ffmpeg -f x11grab -video_size 1080x1920 -i :0.0 -vframes 1 "vpxbct-tmp-boxart.png"

      string szSize = std::to_string(backglassSize.width) + "x" + std::to_string(backglassSize.height);
      string szPosition = ":0.0+" + std::to_string(tableSize.width) + ",0";

      const char* command_line[] = {"/usr/bin/ffmpeg",
                                    "-y",
                                    "-f", "x11grab",
                                    "-video_size", szSize.c_str(),
                                    "-i", szPosition.c_str(),
                                    "-vframes", "1",
                                    "-compression_level", "9",
                                    szPath.c_str(),
                                    NULL};
      const char* environment[] = {"DISPLAY=:0.0",  NULL};

      struct subprocess_s subprocess;
      if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
         int process_return;
         if (!subprocess_join(&subprocess, &process_return)) {
            PLOGI.printf("process returned: %d", process_return);
            if (!process_return && ProcessPNG(szPath)) {
               struct mg_http_serve_opts opts = {};
               mg_http_serve_file(c, hm, szPath.c_str(), &opts);
               return;
            }
         }
      }

      mg_http_reply(c, 500, "", "Server error");
   }
   else {
      mg_http_reply(c, 400, "", "Bad request");
   }
}

void VPXBatoceraCaptureTool::GenerateDescription(struct mg_connection *c, void *ev_data)
{
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   if (m_szOpenAIKey.empty()) {
      mg_http_reply(c, 404, "", "OpenAI key not set");
      return;
   }

   char name[256];
   mg_http_get_var(&hm->query, "name", name, sizeof(name));

   if (*name == '\0') {
      mg_http_reply(c, 400, "", "Missing name");
      return;
   }

   string systemPrompt = 
      "You are a metadata generator for pinball tables. Never speak conversationally. Only output structured, factual descriptions. "
      "Never ask questions or invite follow-up. Do not use Markdown or formatting. Limit the response to one or two plain-text paragraphs.";

   string prompt = "Write a factual description of the pinball machine titled '" + string(name) +
      "'. Only include details—such as theme, gameplay, artwork, or sound—if they are confirmed from known sources. " +
      "Do not assume or infer anything, even if the name resembles a real machine. " +
      "If you cannot confirm the machine's authenticity or details, respond clearly that no verified information is available.";

   char jsonPayload[4096];
   mg_snprintf(jsonPayload, sizeof(jsonPayload),
       "{ \"model\": \"gpt-4o-mini\", \"messages\": ["
       "{ \"role\": \"system\", \"content\": %m },"
       "{ \"role\": \"user\", \"content\": %m }"
       "] }",
       mg_print_esc, 0, systemPrompt.c_str(),
       mg_print_esc, 0, prompt.c_str());

   PLOGI.printf("JSON Payload: %s", jsonPayload);

   string szPath = m_szBasePath + "vpxbct_gpt_request.json";

   FILE* f = fopen(szPath.c_str(), "w");
   if (!f) {
      mg_http_reply(c, 500, "", "Failed to write temp request file");
      return;
   }
   fputs(jsonPayload, f);
   fclose(f);

   string authHeader = "Authorization: Bearer " + m_szOpenAIKey;
   string dataArg = "@" + string(szPath);

   const char* command_line[] = {
      "/usr/bin/curl",
      "-s",
      "-X", "POST",
      "-H", "Content-Type: application/json",
      "-H", authHeader.c_str(),
      "-d", dataArg.c_str(),
      "https://api.openai.com/v1/chat/completions",
      NULL
   };

   const char* env[] = { nullptr };
   struct subprocess_s subprocess;

   if (subprocess_create_ex(command_line, 0, env, &subprocess) != 0) {
      mg_http_reply(c, 500, "", "Failed to start subprocess");
      return;
   }

   int retcode = 0;
   subprocess_join(&subprocess, &retcode);

   FILE* out = subprocess_stdout(&subprocess);
   if (!out) {
      subprocess_destroy(&subprocess);
      mg_http_reply(c, 500, "", "Error retrieving subprocess output");
      return;
   }

   char buffer[2048];
   string response;
   while (fgets(buffer, sizeof(buffer), out)) {
      response += buffer;
   }

   subprocess_destroy(&subprocess);

   struct mg_str jsonStr = mg_str(response.c_str());
   char* description = mg_json_get_str(jsonStr, "$.choices[0].message.content");

   if (description) {
      PLOGI.printf("Response: description=%s", description);

      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%m: %m}",
                    MG_ESC("description"), MG_ESC(description));
      free(description);
   }
   else {
      PLOGE.printf("Error: response=%s", response.c_str());

      mg_http_reply(c, 500, "", "Failed to parse description");
   }
}

void VPXBatoceraCaptureTool::LoadINI()
{
   string szIniFile = m_szBasePath + "vpxbct.ini";

   PLOGI.printf("Loading ini at: %s", szIniFile.c_str());

   mINI::INIFile file(szIniFile);
   mINI::INIStructure ini;
   file.read(ini);

   if (!ini.has("Settings")) {
      PLOGE << "Missing settings entry";
      return;
   }

   if (ini["Settings"].has("ESURL"))
      m_szESUrl = ini["Settings"]["ESURL"];

   if (ini["Settings"].has("OpenAIKey"))
      m_szOpenAIKey = ini["Settings"]["OpenAIKey"];
}

bool VPXBatoceraCaptureTool::GetScreenResolution(int screenNum, ScreenSize* outSize) {
   if (!outSize || screenNum < 1 || screenNum > 3)
      return false;

   string index = (screenNum == 1) ? "" : std::to_string(screenNum);
   string script =
       "output=$(batocera-settings-get global.videooutput" + index + ")\n"
       "if [ -n \"$output\" ]; then\n"
       "  batocera-resolution --screen \"$output\" currentResolution\n"
       "fi\n";

   const char* command_line[] = { "/bin/bash", "-c", script.c_str(), NULL };
   const char* environment[] = { "DISPLAY=:0.0", NULL };

   struct subprocess_s subprocess;

   if (subprocess_create_ex(command_line, 0, environment, &subprocess) != 0)
      return false;

   if (subprocess_join(&subprocess, nullptr) != 0)
      return false;

   FILE* out = subprocess_stdout(&subprocess);
   if (!out)
      return false;

   char line[256];
   if (fgets(line, sizeof(line), out)) {
      string output(line);
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      size_t xPos = output.find('x');
      if (xPos != string::npos) {
         outSize->width = std::stoi(output.substr(0, xPos));
         outSize->height = std::stoi(output.substr(xPos + 1));
         PLOGI.printf("screenNum=%d, size:%dx%d", screenNum, outSize->width, outSize->height);
         subprocess_destroy(&subprocess);
         return true;
      }
   }

   subprocess_destroy(&subprocess);
   return false;
}

bool VPXBatoceraCaptureTool::ProcessPNG(const string& filename)
{
   unsigned int width;
   unsigned int height;
   unsigned char* pPixels;

   if (lodepng_decode32_file(&pPixels, &width, &height, filename.c_str()))
      return false;

   liq_attr* pHandle = liq_attr_create();
   liq_image* pImage = liq_image_create_rgba(pHandle, pPixels, width, height, 0);

   liq_result* pQuantizationResult;
   if (liq_image_quantize(pImage, pHandle, &pQuantizationResult) != LIQ_OK)
      return false;

   size_t pixelsSize = width * height;
   unsigned char* pPixels8 = (unsigned char *)malloc(pixelsSize);
   liq_set_dithering_level(pQuantizationResult, 1.0);

   liq_write_remapped_image(pQuantizationResult, pImage, pPixels8, pixelsSize);
   const liq_palette* pPalette = liq_get_palette(pQuantizationResult);

   LodePNGState state;
   lodepng_state_init(&state);
   state.info_raw.colortype = LCT_PALETTE;
   state.info_raw.bitdepth = 8;
   state.info_png.color.colortype = LCT_PALETTE;
   state.info_png.color.bitdepth = 8;

   for (int i = 0; i < pPalette->count; i++) {
      lodepng_palette_add(&state.info_png.color, pPalette->entries[i].r, pPalette->entries[i].g, pPalette->entries[i].b, pPalette->entries[i].a);
      lodepng_palette_add(&state.info_raw, pPalette->entries[i].r, pPalette->entries[i].g, pPalette->entries[i].b, pPalette->entries[i].a);
   }

   unsigned char* pOutput;
   size_t outputSize;
   if (lodepng_encode(&pOutput, &outputSize, pPixels8, width, height, &state))
      return false;

   FILE *fp = fopen(filename.c_str(), "wb");
   if (!fp)
      return false;

   fwrite(pOutput, 1, outputSize, fp);
   fclose(fp);

   liq_result_destroy(pQuantizationResult);
   liq_image_destroy(pImage);
   liq_attr_destroy(pHandle);

   free(pPixels8);
   lodepng_state_cleanup(&state);

   return true;
}

int VPXBatoceraCaptureTool::Start()
{
   PLOGI << "Starting VPX Batocera Capture Tool...";

   LoadINI();

   PLOGI << "Starting server started on port 8111";

   g_pServer = this;

   struct mg_mgr mgr;
   struct mg_connection* conn;
   mg_mgr_init(&mgr);

   mg_http_listen(&mgr, "http://0.0.0.0:8111", VPXBatoceraCaptureTool::HandleEvent, NULL);

   if (m_szESUrl.empty())
      m_szESUrl = "http://127.0.0.1:1234";

   PLOGI << "ES URL: " << m_szESUrl;

   bool quit = false;

   while (!quit) {
      mg_mgr_poll(&mgr, 1000);
   }

   mg_mgr_free(&mgr);

   return 0;
}
