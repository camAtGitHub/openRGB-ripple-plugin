import { Download } from "lucide-react";
import { KeyboardStage } from "@/components/keyboard-stage";
import { Controls } from "@/components/controls";
import { InstallPanel } from "@/components/install-panel";
import { Button } from "@/components/ui/button";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";

export function Studio() {
  return (
    <div className="min-h-dvh bg-bg text-fg">
      <header className="border-b border-border">
        <div className="mx-auto flex max-w-[1400px] items-center justify-between gap-4 px-4 py-4 sm:px-6">
          <div className="min-w-0">
            <p className="font-mono text-[0.62rem] uppercase tracking-[0.22em] text-accent">
              OpenRGB · Plugin API 4
            </p>
            <h1 className="font-display text-xl font-semibold tracking-tight sm:text-2xl">
              Ripple
            </h1>
          </div>
          <Button asChild variant="accent" size="sm">
            <a href="https://github.com/camAtGitHub/openRGB-ripple-plugin/releases/latest/download/OpenRGBRipplePlugin.dll">
              <Download className="size-4" />
              Download plugin
            </a>
          </Button>
        </div>
      </header>

      <main className="mx-auto max-w-[1400px] px-4 py-6 sm:px-6 sm:py-8">
        <Tabs defaultValue="studio">
          <TabsList className="mb-6">
            <TabsTrigger value="studio">Studio</TabsTrigger>
            <TabsTrigger value="install">Install</TabsTrigger>
          </TabsList>
          <TabsContent value="studio">
            <div className="flex flex-col gap-6 lg:flex-row lg:items-start">
              <div className="flex min-w-0 flex-1 items-center justify-center rounded-xl border border-border bg-bg-panel p-4 sm:p-6">
                <KeyboardStage />
              </div>
              <Controls />
            </div>
          </TabsContent>
          <TabsContent value="install">
            <InstallPanel />
          </TabsContent>
        </Tabs>
      </main>
    </div>
  );
}
