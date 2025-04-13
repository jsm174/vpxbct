#include <string>
#include <vector>

using std::string;
using std::vector;

struct ScreenSize {
   int width;
   int height;
};

class VPXBatoceraCaptureTool
{
public:
   VPXBatoceraCaptureTool();
   ~VPXBatoceraCaptureTool();

   static void Forward(struct mg_http_message *hm, struct mg_connection *c);
   static void HandleEvent(struct mg_connection *c, int ev, void *ev_data);
   static void HandleEvent2(struct mg_connection *c, int ev, void *ev_data);

   int Start();
   void SetBasePath(const string& path) { m_szBasePath = path; }

private:
   void LoadINI();
   void Capture(struct mg_connection *c, void *ev_data);
   void GenerateDescription(struct mg_connection *c, void *ev_data);
   bool GetScreenResolution(int screenNum, ScreenSize* outSize);
   bool ProcessPNG(const std::string& filename);

   string m_szBasePath;
   string m_szESUrl;
   string m_szOpenAIKey;
};