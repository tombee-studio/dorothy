/* Copyright 2026(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/ast.hpp"
#include "../include/lexer.hpp"
#include "../include/llvm_gen.hpp"
#include "../include/parser.hpp"

static int run_llvm(const std::string &source) {
  Lexer lexer;
  Parser parser;
  auto tokens = lexer.lex(source);
  auto program = parser.parse(tokens);

  char tmp_ll[] = "/tmp/dorothy_test_glinda_XXXXXX.ll";
  char tmp_bin[] = "/tmp/dorothy_test_glinda_bin_XXXXXX";
  int fd_ll = mkstemps(tmp_ll, 3);
  int fd_bin = mkstemp(tmp_bin);
  if (fd_ll >= 0)
    close(fd_ll);
  if (fd_bin >= 0)
    close(fd_bin);

  std::ofstream ofs(tmp_ll);
  LLVMGenCtx ctx(ofs);
  for (auto func : program) {
    if (!func->isImport())
      ctx.defined_funcs.insert(func->getName());
  }
  for (auto func : program) {
    func->llvm_pre_register(ctx);
  }
  for (auto func : program) {
    func->llvm_emit(ctx);
  }
  ofs.close();

  std::string err_file = std::string(tmp_bin) + ".err";
  std::string cmd = "clang -Wno-override-module -o " + std::string(tmp_bin) +
                    " " + std::string(tmp_ll) + " >" + err_file + " 2>&1";
  int compile_res = system(cmd.c_str());
  if (compile_res != 0) {
    std::ifstream efs(err_file);
    std::string err_str((std::istreambuf_iterator<char>(efs)),
                        std::istreambuf_iterator<char>());
    std::ifstream lfs(tmp_ll);
    std::string ll_str((std::istreambuf_iterator<char>(lfs)),
                       std::istreambuf_iterator<char>());
    remove(tmp_ll);
    remove(tmp_bin);
    remove(err_file.c_str());
    throw std::runtime_error("clang compilation failed:\n" + err_str +
                             "\nGenerated LLVM IR:\n" + ll_str);
  }
  remove(err_file.c_str());

  int exec_res = system(tmp_bin);
  int exit_code = WEXITSTATUS(exec_res);

  remove(tmp_ll);
  remove(tmp_bin);
  return exit_code;
}

// 1. Componentの基本ライフサイクル（init, update, render）の動作検証
TEST(GlindaTest, ComponentLifecycle) {
  std::string src =
      "import \"string.h\";\n"
      "abstract class Component {\n"
      "    var entity: Entity;\n"
      "    var next: Component;\n"
      "    abstract func getName() -> long;\n"
      "    abstract func init();\n"
      "    abstract func update();\n"
      "    abstract func render(ren: long);\n"
      "}\n"
      "class Entity {\n"
      "    var active: int;\n"
      "    var firstComponent: Component;\n"
      "    var next: Entity;\n"
      "    func Entity() {\n"
      "        this.active = 1;\n"
      "        this.firstComponent = 0;\n"
      "        this.next = 0;\n"
      "    }\n"
      "    func addComponent(c: Component) {\n"
      "        c.entity = this;\n"
      "        c.init();\n"
      "        if (this.firstComponent == 0) {\n"
      "            this.firstComponent = c;\n"
      "        } else {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr.next != 0) {\n"
      "                curr = curr.next;\n"
      "            }\n"
      "            curr.next = c;\n"
      "        }\n"
      "    }\n"
      "    func update() {\n"
      "        if (this.active == 1) {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr != 0) {\n"
      "                curr.update();\n"
      "                curr = curr.next;\n"
      "            }\n"
      "        }\n"
      "    }\n"
      "    func render(ren: long) {\n"
      "        if (this.active == 1) {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr != 0) {\n"
      "                curr.render(ren);\n"
      "                curr = curr.next;\n"
      "            }\n"
      "        }\n"
      "    }\n"
      "}\n"
      "class MockComponent: Component {\n"
      "    var initCount: int;\n"
      "    var updateCount: int;\n"
      "    var renderCount: int;\n"
      "    func MockComponent() {\n"
      "        this.initCount = 0;\n"
      "        this.updateCount = 0;\n"
      "        this.renderCount = 0;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Mock\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {\n"
      "        this.initCount = this.initCount + 1;\n"
      "    }\n"
      "    override func update() {\n"
      "        this.updateCount = this.updateCount + 1;\n"
      "    }\n"
      "    override func render(ren: long) {\n"
      "        this.renderCount = this.renderCount + 1;\n"
      "    }\n"
      "}\n"
      "func main() -> int {\n"
      "    var e: Entity = Entity();\n"
      "    var m: MockComponent = MockComponent();\n"
      "    e.addComponent(m);\n"
      "    e.update();\n"
      "    e.update();\n"
      "    e.render(0);\n"
      "    // initCount=1, updateCount=2, renderCount=1 -> 1*100 + 2*10 + 1 = "
      "121\n"
      "    return m.initCount * 100 + m.updateCount * 10 + m.renderCount;\n"
      "}\n";
  EXPECT_EQ(run_llvm(src), 121);
}

// 2. getComponent によるコンポーネント取得と操作
TEST(GlindaTest, GetComponentByName) {
  std::string src =
      "import \"string.h\";\n"
      "abstract class Component {\n"
      "    var entity: Entity;\n"
      "    var next: Component;\n"
      "    abstract func getName() -> long;\n"
      "    abstract func init();\n"
      "    abstract func update();\n"
      "    abstract func render(ren: long);\n"
      "}\n"
      "class Entity {\n"
      "    var active: int;\n"
      "    var firstComponent: Component;\n"
      "    var next: Entity;\n"
      "    func Entity() {\n"
      "        this.active = 1;\n"
      "        this.firstComponent = 0;\n"
      "        this.next = 0;\n"
      "    }\n"
      "    func addComponent(c: Component) {\n"
      "        c.entity = this;\n"
      "        c.init();\n"
      "        if (this.firstComponent == 0) {\n"
      "            this.firstComponent = c;\n"
      "        } else {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr.next != 0) {\n"
      "                curr = curr.next;\n"
      "            }\n"
      "            curr.next = c;\n"
      "        }\n"
      "    }\n"
      "    func getComponent(name: long) -> Component {\n"
      "        var curr: Component = this.firstComponent;\n"
      "        while (curr != 0) {\n"
      "            var cname: long = curr.getName();\n"
      "            if (strcmp(cname, name) == 0) {\n"
      "                return curr;\n"
      "            }\n"
      "            curr = curr.next;\n"
      "        }\n"
      "        return 0;\n"
      "    }\n"
      "    func update() {}\n"
      "    func render(ren: long) {}\n"
      "}\n"
      "class PositionComponent: Component {\n"
      "    var x: int;\n"
      "    var y: int;\n"
      "    func PositionComponent(x: int, y: int) {\n"
      "        this.x = x;\n"
      "        this.y = y;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Position\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {}\n"
      "    override func update() {}\n"
      "    override func render(ren: long) {}\n"
      "}\n"
      "class HealthComponent: Component {\n"
      "    var hp: int;\n"
      "    func HealthComponent(hp: int) {\n"
      "        this.hp = hp;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Health\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {}\n"
      "    override func update() {}\n"
      "    override func render(ren: long) {}\n"
      "}\n"
      "func main() -> int {\n"
      "    var e: Entity = Entity();\n"
      "    var pos: PositionComponent = PositionComponent(100, 200);\n"
      "    var hp: HealthComponent = HealthComponent(50);\n"
      "    e.addComponent(pos);\n"
      "    e.addComponent(hp);\n"
      "    var posName = \"Position\";\n"
      "    var hpName = \"Health\";\n"
      "    var foundPos: PositionComponent = e.getComponent(posName);\n"
      "    var foundHp: HealthComponent = e.getComponent(hpName);\n"
      "    return foundPos.x + foundHp.hp; // 100 + 50 = 150\n"
      "}\n";
  EXPECT_EQ(run_llvm(src), 150);
}

// 3. Component内から this.entity.getComponent 経由で他Componentを連携操作
TEST(GlindaTest, InterComponentAccessViaEntity) {
  std::string src =
      "import \"string.h\";\n"
      "abstract class Component {\n"
      "    var entity: Entity;\n"
      "    var next: Component;\n"
      "    abstract func getName() -> long;\n"
      "    abstract func init();\n"
      "    abstract func update();\n"
      "    abstract func render(ren: long);\n"
      "}\n"
      "class Entity {\n"
      "    var active: int;\n"
      "    var firstComponent: Component;\n"
      "    var next: Entity;\n"
      "    func Entity() {\n"
      "        this.active = 1;\n"
      "        this.firstComponent = 0;\n"
      "        this.next = 0;\n"
      "    }\n"
      "    func addComponent(c: Component) {\n"
      "        c.entity = this;\n"
      "        c.init();\n"
      "        if (this.firstComponent == 0) {\n"
      "            this.firstComponent = c;\n"
      "        } else {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr.next != 0) {\n"
      "                curr = curr.next;\n"
      "            }\n"
      "            curr.next = c;\n"
      "        }\n"
      "    }\n"
      "    func getComponent(name: long) -> Component {\n"
      "        var curr: Component = this.firstComponent;\n"
      "        while (curr != 0) {\n"
      "            var cname: long = curr.getName();\n"
      "            if (strcmp(cname, name) == 0) {\n"
      "                return curr;\n"
      "            }\n"
      "            curr = curr.next;\n"
      "        }\n"
      "        return 0;\n"
      "    }\n"
      "    func update() {\n"
      "        var curr: Component = this.firstComponent;\n"
      "        while (curr != 0) {\n"
      "            curr.update();\n"
      "            curr = curr.next;\n"
      "        }\n"
      "    }\n"
      "    func render(ren: long) {}\n"
      "}\n"
      "class TransformComponent: Component {\n"
      "    var x: int;\n"
      "    var y: int;\n"
      "    func TransformComponent(x: int, y: int) {\n"
      "        this.x = x;\n"
      "        this.y = y;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Transform\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {}\n"
      "    override func update() {}\n"
      "    override func render(ren: long) {}\n"
      "}\n"
      "class MovementComponent: Component {\n"
      "    var speedX: int;\n"
      "    var speedY: int;\n"
      "    func MovementComponent(sx: int, sy: int) {\n"
      "        this.speedX = sx;\n"
      "        this.speedY = sy;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Movement\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {}\n"
      "    override func update() {\n"
      "        var tName = \"Transform\";\n"
      "        var t: TransformComponent = this.entity.getComponent(tName);\n"
      "        if (t != 0) {\n"
      "            t.x = t.x + this.speedX;\n"
      "            t.y = t.y + this.speedY;\n"
      "        }\n"
      "    }\n"
      "    override func render(ren: long) {}\n"
      "}\n"
      "func main() -> int {\n"
      "    var e: Entity = Entity();\n"
      "    var trans: TransformComponent = TransformComponent(10, 20);\n"
      "    var move: MovementComponent = MovementComponent(3, 7);\n"
      "    e.addComponent(trans);\n"
      "    e.addComponent(move);\n"
      "    e.update();\n"
      "    e.update();\n"
      "    // x = 10 + 3*2 = 16, y = 20 + 7*2 = 34 -> 16 + 34 = 50\n"
      "    return trans.x + trans.y;\n"
      "}\n";
  EXPECT_EQ(run_llvm(src), 50);
}

// 4. Scene による複数 Entity の一括管理
TEST(GlindaTest, SceneEntityBatchExecution) {
  std::string src =
      "import \"string.h\";\n"
      "abstract class Component {\n"
      "    var entity: Entity;\n"
      "    var next: Component;\n"
      "    abstract func getName() -> long;\n"
      "    abstract func init();\n"
      "    abstract func update();\n"
      "    abstract func render(ren: long);\n"
      "}\n"
      "class Entity {\n"
      "    var active: int;\n"
      "    var firstComponent: Component;\n"
      "    var next: Entity;\n"
      "    func Entity() {\n"
      "        this.active = 1;\n"
      "        this.firstComponent = 0;\n"
      "        this.next = 0;\n"
      "    }\n"
      "    func addComponent(c: Component) {\n"
      "        c.entity = this;\n"
      "        c.init();\n"
      "        if (this.firstComponent == 0) {\n"
      "            this.firstComponent = c;\n"
      "        } else {\n"
      "            var curr: Component = this.firstComponent;\n"
      "            while (curr.next != 0) {\n"
      "                curr = curr.next;\n"
      "            }\n"
      "            curr.next = c;\n"
      "        }\n"
      "    }\n"
      "    func update() {\n"
      "        var curr: Component = this.firstComponent;\n"
      "        while (curr != 0) {\n"
      "            curr.update();\n"
      "            curr = curr.next;\n"
      "        }\n"
      "    }\n"
      "    func render(ren: long) {}\n"
      "}\n"
      "class CounterComponent: Component {\n"
      "    var val: int;\n"
      "    func CounterComponent(v: int) {\n"
      "        this.val = v;\n"
      "    }\n"
      "    override func getName() -> long {\n"
      "        var n = \"Counter\";\n"
      "        return n;\n"
      "    }\n"
      "    override func init() {}\n"
      "    override func update() {\n"
      "        this.val = this.val + 1;\n"
      "    }\n"
      "    override func render(ren: long) {}\n"
      "}\n"
      "class Scene {\n"
      "    var firstEntity: Entity;\n"
      "    func Scene() {\n"
      "        this.firstEntity = 0;\n"
      "    }\n"
      "    func addEntity(e: Entity) {\n"
      "        if (this.firstEntity == 0) {\n"
      "            this.firstEntity = e;\n"
      "        } else {\n"
      "            var curr: Entity = this.firstEntity;\n"
      "            while (curr.next != 0) {\n"
      "                curr = curr.next;\n"
      "            }\n"
      "            curr.next = e;\n"
      "        }\n"
      "    }\n"
      "    func update() {\n"
      "        var curr: Entity = this.firstEntity;\n"
      "        while (curr != 0) {\n"
      "            curr.update();\n"
      "            curr = curr.next;\n"
      "        }\n"
      "    }\n"
      "    func render(ren: long) {}\n"
      "}\n"
      "func main() -> int {\n"
      "    var scene: Scene = Scene();\n"
      "    var e1: Entity = Entity();\n"
      "    var c1: CounterComponent = CounterComponent(10);\n"
      "    e1.addComponent(c1);\n"
      "    var e2: Entity = Entity();\n"
      "    var c2: CounterComponent = CounterComponent(20);\n"
      "    e2.addComponent(c2);\n"
      "    scene.addEntity(e1);\n"
      "    scene.addEntity(e2);\n"
      "    scene.update();\n"
      "    scene.update();\n"
      "    scene.update();\n"
      "    // c1 = 10 + 3 = 13, c2 = 20 + 3 = 23 -> 13 + 23 = 36\n"
      "    return c1.val + c2.val;\n"
      "}\n";
  EXPECT_EQ(run_llvm(src), 36);
}
