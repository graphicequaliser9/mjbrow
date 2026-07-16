// Standalone smoke test for js::QuickJSContext + DOM host objects.
// Verifies the use-after-free fixes and basic DOM binding behaviour.

#include "js/QuickJSContext.h"
#include "html/DOMNode.h"

#include <cassert>
#include <iostream>
#include <memory>

using namespace js;
using namespace html;

int main() {
    // Build a tiny document: <div id="x"><p class="c">hi</p></div>
    auto doc = std::make_unique<Document>();
    auto* div = new DOMNode();
    div->nodeType = NodeType::Element;
    div->tagName = "div";
    div->setAttribute("id", "x");
    auto* p = new DOMNode();
    p->nodeType = NodeType::Element;
    p->tagName = "p";
    p->setAttribute("class", "c");
    p->textContent = "hi";
    div->appendChild(p);
    doc->appendChild(div);

    QuickJSContext ctx;
    ctx.setDocument(doc.get());

    // getElementById returns a wrapped node, not a crash.
    std::string r1 = ctx.evaluate("typeof document.getElementById('x')");
    std::cout << "getElementById type: " << r1 << "\n";
    assert(r1 == "object");

    // querySelector returns a wrapped node.
    std::string r2 = ctx.evaluate("typeof document.querySelector('.c')");
    std::cout << "querySelector type: " << r2 << "\n";
    assert(r2 == "object");

    // querySelectorAll returns an array.
    std::string r3 = ctx.evaluate("document.querySelectorAll('p').length");
    std::cout << "querySelectorAll length: " << r3 << "\n";
    assert(r3 == "1");

    // hasException resets across calls (fix #5).
    ctx.evaluate("throw new Error('boom')");
    assert(ctx.hasException());
    ctx.evaluate("1 + 1");
    assert(!ctx.hasException());

    // executePendingJobs drains microtasks without error.
    ctx.evaluate("Promise.resolve().then(() => 1)");
    int jobs = ctx.executePendingJobs();
    std::cout << "pending jobs executed: " << jobs << "\n";
    assert(jobs >= 0);

    std::cout << "ALL QUICKJS CONTEXT TESTS PASSED\n";
    return 0;
}
