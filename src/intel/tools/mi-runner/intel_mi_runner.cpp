#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "dev/gen_device_info.h"
#include "gen_mi_runner.h"
#include "i915_pipe_data.h"
#include "util/list.h"
#include "util/macros.h"

struct child_bo {
   uint64_t gtt_offset;
   uint64_t size;
   void *map;
   int fd;

   struct list_head link;
};

static inline uint64_t
align_down_u64(uint64_t v, uint64_t a)
{
   assert(a != 0 && a == (a & -a));
   return v & ~(a - 1);
}

static inline uint64_t
align_u64(uint64_t v, uint64_t a)
{
   return align_down_u64(v + a - 1, a);
}

static mi_runner_exec
get_mi_runner_exec_for_devinfo(struct gen_device_info *devinfo)
{
   switch (devinfo->gen) {
   case 7:
      if (devinfo->is_haswell)
         return gen75_mi_runner_execute_one_inst;
      return gen7_mi_runner_execute_one_inst;
   case 8:
      return gen8_mi_runner_execute_one_inst;
   case 9:
      return gen9_mi_runner_execute_one_inst;
   case 10:
      return gen10_mi_runner_execute_one_inst;
   case 11:
      return gen11_mi_runner_execute_one_inst;
   case 12:
      return gen12_mi_runner_execute_one_inst;
   }

   return NULL;
}

#define zalloc(__type) ((__typeof(__type)*)calloc(1, sizeof(__type)))

/* UI */

#include <gio/gunixconnection.h>

#include <epoxy/gl.h>

#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"
#include "imgui_impl_gtk3.h"
#include "imgui_impl_opengl3.h"

#include "aubinator_viewer.h"
#include "aubinator_viewer_urb.h"

struct window {
   struct list_head link; /* link in the global list of windows */
   struct list_head parent_link; /* link in parent window list of children */

   struct list_head children_windows; /* list of children windows */

   char name[128];
   bool opened;

   ImVec2 position;
   ImVec2 size;

   void (*display)(struct window*);
   void (*destroy)(struct window*);
};

struct memory_window {
   struct window base;

   uint64_t gtt_offset;
   struct child_bo *bo;

   struct list_head memory_link;

   struct MemoryEditor editor;
};

struct batch_window {
   struct window base;

   uint64_t batch_address;
   bool ppgtt;

   struct aub_viewer_decode_cfg decode_cfg;
   struct aub_viewer_decode_ctx decode_ctx;

   char edit_address[20];
};

static struct Context {
   uint32_t device_id;
   struct gen_device_info devinfo;
   struct gen_spec *spec;

   struct gen_mi_context mi_context;
   mi_runner_exec mi_exec;

   struct {
      bool enabled;
      int steps;
   } mi_exec_runfree;

   struct {
      bool enabled;
      uint64_t address;
      bool ppgtt;
   } mi_exec_runupto;

   bool clean_on_next_bo;
   struct list_head child_bos;

   struct child_bo *batch_bo;

   int child_sockets[2];
   pid_t child_pid;

   GSocketConnection *child_connection;
   GSource *child_source;

   GtkWidget *gtk_window;

   /* UI state*/
   struct aub_viewer_cfg cfg;

   struct list_head windows;
   struct list_head memory_windows;

   struct window engine_window;
   struct window buffers_window;
} context;

thread_local ImGuiContext* __MesaImGui;

static int
map_key(int k)
{
   return ImGuiKey_COUNT + k;
}

static bool
has_ctrl_key(int key)
{
   return ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(map_key(key));
}

static bool
window_has_ctrl_key(int key)
{
   return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && has_ctrl_key(key);
}

/* Memory view window */

static void
display_memory_window(struct window *win)
{
   struct memory_window *window = (struct memory_window *) win;

   if (!window->bo) {
      list_for_each_entry(struct child_bo, bo, &context.child_bos, link) {
         if (window->gtt_offset == bo->gtt_offset) {
            window->bo = bo;
            break;
         }
      }
   }

   if (window->bo) {
      window->editor.DrawContents(window->bo->map,
                                  window->bo->size,
                                  window->bo->gtt_offset);
   } else {
      ImGui::Text("No BO at address=0x%012" PRIx64, window->gtt_offset);
   }
}

static void
destroy_memory_window(struct window *win)
{
   struct memory_window *window = (struct memory_window *) win;

   list_del(&window->memory_link);
   free(win);
}

static void
new_memory_window(struct child_bo *bo)
{
   struct memory_window *window = zalloc(*window);

   snprintf(window->base.name, sizeof(window->base.name),
            "Memory view##%p", window);

   list_inithead(&window->base.parent_link);
   list_inithead(&window->base.children_windows);
   window->base.position = ImVec2(-1, -1);
   window->base.size = ImVec2(600, 700);
   window->base.opened = true;
   window->base.display = display_memory_window;
   window->base.destroy = destroy_memory_window;

   window->bo = bo;
   window->gtt_offset = bo->gtt_offset;

   window->editor = MemoryEditor();
   window->editor.OptShowDataPreview = true;
   window->editor.OptShowAscii = false;

   list_addtail(&window->base.link, &context.windows);
   list_addtail(&window->memory_link, &context.memory_windows);
}

/* Batch decoding window */

static void
display_decode_options(struct aub_viewer_decode_cfg *cfg)
{
   char name[40];
   snprintf(name, sizeof(name), "command filter##%p", &cfg->command_filter);
   cfg->command_filter.Draw(name); ImGui::SameLine();
   snprintf(name, sizeof(name), "field filter##%p", &cfg->field_filter);
   cfg->field_filter.Draw(name); ImGui::SameLine();
   if (ImGui::Button("Dwords")) cfg->show_dwords ^= 1;
}

static struct gen_batch_decode_bo
decode_get_bo(void *user_data, bool ppgtt, uint64_t address)
{
   struct gen_batch_decode_bo no_bo = {0};

   if (!ppgtt)
      return no_bo;

   list_for_each_entry(struct child_bo, bo, &context.child_bos, link) {
      if (address >= bo->gtt_offset &&
          address < (bo->gtt_offset + bo->size)) {
         struct gen_batch_decode_bo decode_bo;
         decode_bo.map = bo->map;
         decode_bo.addr = bo->gtt_offset;
         decode_bo.size = bo->size;
         return decode_bo;
      }
   }

   return no_bo;
}

static void
display_batch_window(struct window *win)
{
   struct batch_window *window = (struct batch_window *) win;

   ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / (2 * 2));
   if (window_has_ctrl_key('f')) ImGui::SetKeyboardFocusHere();
   display_decode_options(&window->decode_cfg);
   ImGui::PopItemWidth();

   uint64_t gtt_offset =
      align_down_u64(context.mi_context.pc[context.mi_context.pc_depth], 4096);
   ImGui::Text("Decoding 0x%012" PRIx64, gtt_offset);

   ImGui::BeginChild(ImGui::GetID("##block"));

   struct gen_batch_decode_bo bo =
      decode_get_bo(NULL,
                    context.mi_context.pc_as[context.mi_context.pc_depth],
                    gtt_offset);

   window->decode_ctx.current_pc =
      context.mi_context.pc[context.mi_context.pc_depth];

   if (bo.map) {
      uint64_t rel_offset = gtt_offset - bo.addr;
      uint64_t rel_size = bo.size - rel_offset;
      aub_viewer_render_batch(&window->decode_ctx,
                              (uint8_t *) bo.map + rel_offset,
                              rel_size,
                              bo.addr + rel_offset,
                              false);
   } else {
      ImGui::Text("Current MI %016" PRIx64 " PC is outside of the execution address space",
                  gtt_offset);
   }

   ImGui::EndChild();
}

static void
destroy_batch_window(struct window *win)
{
   struct batch_window *window = (struct batch_window *) win;

   free(window);
}

static void
batch_run_up_to(void *user_data, uint64_t address, bool ppgtt)
{
   context.mi_exec_runupto.enabled = true;
   context.mi_exec_runupto.address = address;
   context.mi_exec_runupto.ppgtt = ppgtt;
}

static void
new_batch_window(void)
{
   struct batch_window *window = zalloc(*window);

   snprintf(window->base.name, sizeof(window->base.name),
            "Batch view##%p", window);

   list_inithead(&window->base.parent_link);
   list_inithead(&window->base.children_windows);
   window->base.position = ImVec2(-1, -1);
   window->base.size = ImVec2(600, 700);
   window->base.opened = true;
   window->base.display = display_batch_window;
   window->base.destroy = destroy_batch_window;

   window->decode_cfg = aub_viewer_decode_cfg();

   aub_viewer_decode_ctx_init(&window->decode_ctx,
                              &context.cfg,
                              &window->decode_cfg,
                              &context.devinfo,
                              context.spec,
                              decode_get_bo,
                              NULL,
                              window);

   window->decode_ctx.run_up_to = batch_run_up_to;

   list_addtail(&window->base.link, &context.windows);
}

/* Buffer list window */

static void
display_buffers_window(struct window *win)
{
   ImGuiColorEditFlags cflags = (ImGuiColorEditFlags_NoAlpha |
                                 ImGuiColorEditFlags_NoLabel |
                                 ImGuiColorEditFlags_NoInputs);
   struct aub_viewer_cfg *cfg = &context.cfg;

   ImGui::ColorEdit3("background", (float *)&cfg->clear_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("missing", (float *)&cfg->missing_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("error", (float *)&cfg->error_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("highlight", (float *)&cfg->highlight_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("dwords", (float *)&cfg->dwords_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("booleans", (float *)&cfg->boolean_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("header", (float *)&cfg->highlight_header_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("header_hovered", (float *)&cfg->highlight_header_hovered_color, cflags); ImGui::SameLine();
   ImGui::ColorEdit3("header_active", (float *)&cfg->highlight_header_active_color, cflags);

   ImGui::Columns(3, "Buffers:");
   ImGui::SetColumnWidth(0, 160);
   ImGui::Text("Address:");
   ImGui::NextColumn();
   ImGui::SetColumnWidth(1, 100);
   ImGui::Text("Size:");
   ImGui::NextColumn();
   ImGui::SetColumnWidth(2, 60);
   ImGui::Text("Edit:");
   ImGui::NextColumn();

   list_for_each_entry(struct child_bo, bo, &context.child_bos, link) {
      ImGui::Text("0x%016" PRIx64, bo->gtt_offset);
      ImGui::NextColumn();
      ImGui::Text("%" PRIu64, bo->size);
      ImGui::NextColumn();
      ImGui::PushID(bo);
      if (ImGui::Button("Edit"))
         new_memory_window(bo);
      ImGui::PopID();
      ImGui::NextColumn();
   }
}

static void
show_buffers_window(void)
{
   struct window *window = &context.buffers_window;

   if (window->opened)
      return;

   snprintf(window->name, sizeof(window->name), "Buffer objects");

   list_inithead(&window->parent_link);
   window->size = ImVec2(-1, 250);
   window->position = ImVec2(0, 0);
   window->opened = true;
   window->display = display_buffers_window;
   window->destroy = NULL;

   list_addtail(&window->link, &context.windows);
}

/* Engine state window */

static void
close_memory_windows(void)
{
   list_for_each_entry_safe(struct memory_window,
                            window, &context.memory_windows, memory_link) {
      window->bo = NULL;
   }
}

static void
clear_child_bos(void)
{
   list_for_each_entry_safe(struct child_bo, bo, &context.child_bos, link) {
      list_del(&bo->link);
      munmap(bo->map, bo->size);
      close(bo->fd);
      free(bo);
   }
}

static void
display_engine_state_window(struct window *win)
{
   struct gen_mi_context *ctx = &context.mi_context;
   bool inst_change = false;

   if (ImGui::Button("Next instruction") ||
       window_has_ctrl_key('n')) {
      context.mi_exec(&context.mi_context);
      inst_change = true;
   } ImGui::SameLine();
   if (ImGui::Button("End batch")) {
      if (!list_is_empty(&context.child_bos)) {
         while (ctx->pc_depth > 0 &&
                context.mi_exec(&context.mi_context) == GEN_MI_RUNNER_STATUS_OK);
      }
      inst_change = true;
   }
   ImGui::SameLine();
   ImGui::Checkbox("Run free", &context.mi_exec_runfree.enabled);
   ImGui::SameLine();
   ImGui::InputInt("Steps", &context.mi_exec_runfree.steps);

   if (context.mi_exec_runupto.enabled) {
      /* Assuming we're always in ppgtt for now. */
      assert(context.mi_exec_runupto.ppgtt);

      while (ctx->pc_depth > 0 &&
             ctx->pc[ctx->pc_depth] != context.mi_exec_runupto.address &&
             context.mi_exec(&context.mi_context) == GEN_MI_RUNNER_STATUS_OK);

      context.mi_exec_runupto.enabled = false;
   } else if (context.mi_exec_runfree.enabled) {
      for (int i = 0; i < context.mi_exec_runfree.steps; i++)
         context.mi_exec(&context.mi_context);
      inst_change = true;

      /* Ensure the UI keeps on redrawing itself to keep on executing
       * instructions.
       */
      ImGui_ImplGtk3_Schedule_NewFrame();
   }

   if (inst_change) {
      /* Returning to the ring level means we've reached the end of the user
       * batch. Notify the child process.
       */
      if (ctx->pc_depth == 0) {
         context.clean_on_next_bo = true;

         struct i915_pipe_execbuf_result_msg exec_result;
         exec_result.base.type = I915_PIPE_MSG_TYPE_EXECBUF_RESULT;
         exec_result.base.size = sizeof(exec_result) - sizeof(struct i915_pipe_base_msg);
         exec_result.result = 0;

         GOutputStream *output_stream =
            g_io_stream_get_output_stream(G_IO_STREAM(context.child_connection));

         GError *error = NULL;
         g_output_stream_write(output_stream,
                               &exec_result, sizeof(exec_result),
                               NULL, &error);
      }
   }

   ImGui::Separator();
   for (uint32_t i = 0; i < ARRAY_SIZE(ctx->pc); i++) {
      ImGui::Text("pc%01u: 0x%012" PRIx64 " (%s)",
                  i, ctx->pc[i], ctx->pc_as[i] ? "PPGTT" : "GGTT");
   }

   ImGui::Separator();
   ImGui::BeginChild("##gpr", ImVec2(0, 150));
   ImGui::Columns(2);
   for (uint32_t i = 0; i < ARRAY_SIZE(ctx->gpr64); i++) {
      if (i > 0 && i % 8 == 0)
         ImGui::NextColumn();
      ImGui::Text("gpr%02u/0x%x: 0x%016" PRIx64,
                  i, 0x2600 + 8 * i, ctx->gpr64[i]);
   }
   ImGui::EndChild();

   ImGui::Separator();
   ImGui::BeginChild("##alu", ImVec2(0, 150));
   ImGui::Columns(2);
   ImGui::Text("predicate:");
   ImGui::Text("src0:    0x%016" PRIx64, ctx->predicate.src0);
   ImGui::Text("src1:    0x%016" PRIx64, ctx->predicate.src1);
   ImGui::Text("result0: 0x%08"  PRIx32, (uint32_t)(ctx->predicate.result & 0xffffffff));
   ImGui::Text("result1: 0x%08"  PRIx32, (uint32_t)(ctx->predicate.result >> 32));
   ImGui::NextColumn();

   ImGui::Text("alu:");
   ImGui::Text("src0: 0x%016" PRIx64, ctx->alu.src0);
   ImGui::Text("src1: 0x%016" PRIx64, ctx->alu.src1);
   ImGui::Text("accu: 0x%016" PRIx64, ctx->alu.accu);
   ImGui::Text("cf:   0x%016" PRIx64, ctx->alu.cf);
   ImGui::Text("zf:   0x%016" PRIx64, ctx->alu.zf);
   ImGui::Text("inst: %u/%u", ctx->alu.inst_idx, ctx->alu.inst_count);
   ImGui::EndChild();
}

static void
show_engine_state_window(void)
{
   struct window *window = &context.engine_window;

   if (window->opened)
      return;

   snprintf(window->name, sizeof(window->name), "Engine state");

   list_inithead(&window->parent_link);
   window->size = ImVec2(-1, 250);
   window->position = ImVec2(0, 0);
   window->opened = true;
   window->display = display_engine_state_window;
   window->destroy = NULL;

   list_addtail(&window->link, &context.windows);
}

/* Main redrawing */

static void
display_windows(void)
{
   /* Start by disposing closed windows, we don't want to destroy windows that
    * have already been scheduled to be painted. So destroy always happens on
    * the next draw cycle, prior to any drawing.
    */
   list_for_each_entry_safe(struct window, window, &context.windows, link) {
      if (window->opened)
         continue;

      /* Can't close this one. */
      if (window == &context.engine_window) {
         window->opened = true;
         continue;
      }

      list_del(&window->link);
      list_del(&window->parent_link);
      if (window->destroy)
         window->destroy(window);
   }

   list_for_each_entry_safe(struct window, window, &context.windows, link) {
      ImGui::SetNextWindowPos(window->position, ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowSize(window->size, ImGuiCond_FirstUseEver);
      if (ImGui::Begin(window->name, &window->opened)) {
         window->display(window);
         window->position = ImGui::GetWindowPos();
         window->size = ImGui::GetWindowSize();
      }
      if (window_has_ctrl_key('w'))
         window->opened = false;
      ImGui::End();
   }
}

static void
repaint_area(GtkGLArea *area, GdkGLContext *gdk_gl_context)
{
   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplGtk3_NewFrame();
   ImGui::NewFrame();

   display_windows();

   ImGui::EndFrame();
   ImGui::Render();

   glClearColor(context.cfg.clear_color.Value.x,
                context.cfg.clear_color.Value.y,
                context.cfg.clear_color.Value.z, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void
realize_area(GtkGLArea *area)
{
   ImGui::CreateContext();
   ImGui_ImplGtk3_Init(GTK_WIDGET(area), true);
   ImGui_ImplOpenGL3_Init("#version 130");

   list_inithead(&context.windows);
   list_inithead(&context.memory_windows);

   new_batch_window();

   ImGui::StyleColorsDark();
   context.cfg = aub_viewer_cfg();

   ImGuiIO& io = ImGui::GetIO();
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

static void
unrealize_area(GtkGLArea *area)
{
   gtk_gl_area_make_current(area);

   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplGtk3_Shutdown();
   ImGui::DestroyContext();
}

static void
size_allocate_area(GtkGLArea *area,
                   GdkRectangle *allocation,
                   gpointer user_data)
{
   if (!gtk_widget_get_realized(GTK_WIDGET(area)))
      return;

   /* We want to catch only initial size allocate. */
   g_signal_handlers_disconnect_by_func(area,
                                        (gpointer) size_allocate_area,
                                        user_data);
   show_engine_state_window();
   show_buffers_window();
}

static void
print_help(const char *progname, const char *error, FILE *file)
{
   fprintf(file,
           "%s"
           "Usage: %s [OPTION]... -- command...\n"
           "Execute MI commands in an interactive debugger.\n\n"
           "      --help             display this help and exit\n"
           "      --platform=name    emulates a given platform (3 letter platform name)\n",
           error ? error : "", progname);
}

static struct gen_mi_bo
exec_get_bo(void *user_data, bool ppgtt, uint64_t address)
{
   struct gen_mi_bo no_bo = {0};

   if (!ppgtt)
      return no_bo;

   list_for_each_entry(struct child_bo, bo, &context.child_bos, link) {
      if (address >= bo->gtt_offset &&
          address < (bo->gtt_offset + bo->size)) {
         struct gen_mi_bo mi_bo;
         mi_bo.map = bo->map;
         mi_bo.gtt_offset = bo->gtt_offset;
         mi_bo.size = bo->size;
         return mi_bo;
      }
   }

   return no_bo;
}

static bool
add_new_bo(const struct i915_pipe_bo_msg *bo_msg)
{
   if (context.clean_on_next_bo) {
      close_memory_windows();
      clear_child_bos();
      context.clean_on_next_bo = false;
   }

   struct child_bo *bo = zalloc(*bo);

   GError *error;
   bo->fd = g_unix_connection_receive_fd(G_UNIX_CONNECTION(context.child_connection),
                                         NULL, &error);
   if (bo->fd < 0) {
      free(bo);
      return false;
   }

   bo->gtt_offset = bo_msg->gtt_offset;
   bo->size = bo_msg->size;
   bo->map = mmap(NULL, bo->size, PROT_WRITE | PROT_READ, MAP_SHARED, bo->fd, bo_msg->mem_addr);
   if (bo->map == MAP_FAILED) {
      close(bo->fd);
      free(bo);
      return false;
   }

   list_add(&bo->link, &context.child_bos);

   ImGui_ImplGtk3_Schedule_NewFrame();

   return true;
}

static void
start_exec(const struct i915_pipe_execbuf_msg *exec_msg)
{
   context.mi_context.get_bo = exec_get_bo;
   context.mi_context.spec = context.spec;
   context.mi_context.engine = I915_ENGINE_CLASS_RENDER;
   context.mi_context.pc[1] = exec_msg->gtt_offset;
   context.mi_context.pc_as[1] = true;
   context.mi_context.pc_depth = 1;
}

static gboolean
child_message_cb(gpointer user_data)
{
   GInputStream *input_stream = g_io_stream_get_input_stream(G_IO_STREAM(context.child_connection));
   union {
      struct i915_pipe_base_msg base;
      struct i915_pipe_bo_msg bo;
      struct i915_pipe_execbuf_msg exec;
   };

   GError *error = NULL;
   gssize res = g_input_stream_read(input_stream, &base, sizeof(base), NULL, &error);
   if (res != sizeof(base))
      return G_SOURCE_REMOVE;

   switch (base.type) {
   case I915_PIPE_MSG_TYPE_BO:
      res = g_input_stream_read(input_stream, &base + 1, base.size, NULL, &error);
      if (res != base.size)
         return G_SOURCE_REMOVE;
      if (!add_new_bo(&bo))
         return G_SOURCE_REMOVE;
      break;

   case I915_PIPE_MSG_TYPE_EXECBUF:
      res = g_input_stream_read(input_stream, &base + 1, base.size, NULL, &error);
      if (res != base.size) {
         g_source_destroy(context.child_source);
         return G_SOURCE_REMOVE;
      }
      start_exec(&exec);
      break;

   default:
      g_source_destroy(context.child_source);
      return G_SOURCE_REMOVE;
   }

   return G_SOURCE_CONTINUE;
}

static bool
file_exists(const char *path)
{
   struct stat st;
   return stat(path, &st) == 0;
}

static void
prepare_child_process(const char *self_path, char *argv[])
{
   int sockets[2];

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
      fprintf(stderr, "Unable to create sockets: %s\n", strerror(errno));
      return;
   }

   context.child_pid = fork();
   if (context.child_pid == -1) {
      fprintf(stderr, "Unable to fork child: %s\n", strerror(errno));
      return;
   }

   if (context.child_pid == 0) {
      /* Child */
      close(sockets[0]);
      /* Assign the socket to FD 2. This is a tacit ag*/
      dup2(sockets[1], 3);
      /* Attempt to file the stub library in the install path or next to our
       * binary (running from the build directory).
       */
      if (file_exists(I915_PIPE_PATH)) {
         setenv("LD_PRELOAD", I915_PIPE_PATH, true);
      } else {
         char *dir = dirname(strdup(self_path));
         char *result = NULL;

         asprintf(&result, "%s/%s", dir, LIBI915_PIPE_NAME);
         setenv("LD_PRELOAD", result, true);
         free(dir);
         free(result);
      }
      char device_id[10];
      snprintf(device_id, sizeof(device_id), "%i", context.device_id);
      setenv("I915_PIPE_DEVICE", device_id, true);
      execv(argv[0], argv);
   }

   close(sockets[1]);

   GError *error = NULL;
   GSocket *socket = g_socket_new_from_fd(sockets[0], &error);
   g_assert_no_error(error);
   context.child_connection = g_socket_connection_factory_create_connection(socket);
   g_assert(context.child_connection != NULL);

   context.child_source = g_pollable_input_stream_create_source(
      G_POLLABLE_INPUT_STREAM(
         g_io_stream_get_input_stream(G_IO_STREAM(context.child_connection))),
      NULL);
   g_source_set_callback(context.child_source, G_SOURCE_FUNC(child_message_cb), NULL, NULL);
   g_source_attach(context.child_source, g_main_context_get_thread_default());
}

int
main(int argc, char *argv[])
{
   int c, sub_args = -1;
   const struct option aubinator_opts[] = {
      { "help",          no_argument,       NULL,  'h' },
      { "platform",      required_argument, NULL,  'p' },
      { NULL,            0,                 NULL,   0  }
   };

   memset(&context, 0, sizeof(context));
   context.mi_exec_runfree.enabled = false;
   context.mi_exec_runfree.steps = 1;

   while ((c = getopt_long(argc, argv, "hp:-", aubinator_opts, NULL)) != -1 && sub_args == -1) {
      switch (c) {
      case 'h':
         print_help(argv[0], NULL, stderr);
         return EXIT_SUCCESS;
      case 'p':
         context.device_id = gen_device_name_to_pci_device_id(optarg);
         if (!gen_get_device_info_from_pci_id(context.device_id,
                                              &context.devinfo))
            print_help(argv[0], "Unknown platform.\n", stderr);
         context.spec = gen_spec_load(&context.devinfo);
         context.mi_exec = get_mi_runner_exec_for_devinfo(&context.devinfo);
         break;
      default:
         break;
      }
   }

   prepare_child_process(argv[0], &argv[optind]);

   list_inithead(&context.child_bos);

   list_inithead(&context.windows);

   gtk_init(NULL, NULL);

   context.gtk_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(context.gtk_window), "Intel MI runner");
   g_signal_connect(context.gtk_window, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
   gtk_window_resize(GTK_WINDOW(context.gtk_window), 1280, 720);

   GtkWidget* gl_area = gtk_gl_area_new();
   g_signal_connect(gl_area, "render", G_CALLBACK(repaint_area), NULL);
   g_signal_connect(gl_area, "realize", G_CALLBACK(realize_area), NULL);
   g_signal_connect(gl_area, "unrealize", G_CALLBACK(unrealize_area), NULL);
   g_signal_connect(gl_area, "size_allocate", G_CALLBACK(size_allocate_area), NULL);
   gtk_container_add(GTK_CONTAINER(context.gtk_window), gl_area);

   gtk_widget_show_all(context.gtk_window);

   gtk_main();

   return 0;
}
