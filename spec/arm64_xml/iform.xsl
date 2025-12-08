<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0" xmlns="http://www.w3.org/1999/xhtml">
<xsl:output doctype-public="-//W3C//DTD XHTML 1.1//EN"
 doctype-system="http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd"
 method="html" encoding="utf-8"/>

<!--
########################################################################
The top-level structure of the page is described here. It matches
relevant parts of the XML file in order to generate a suitable
browseable view.
########################################################################
-->
<xsl:template match="/instructionsection">
    <html>

    <head>
      <link rel="stylesheet" type="text/css" href="insn.css"/>
      <meta name="generator" content="iform.xsl"/>
      <title><xsl:value-of select="@title"/></title>
    </head>

    <body>
    <xsl:call-template name="generate_header_footer"/>
    <hr/>

    <!-- the top-level instruction description -->
    <xsl:apply-templates select="/instructionsection/heading"/>
    <xsl:apply-templates select="/instructionsection/desc"/>
    <xsl:apply-templates select="/instructionsection/alias_list"/>
    <xsl:if test="/instructionsection/aliasto">
      <xsl:variable name="alias_and_or_pseudo">
        <xsl:choose>
          <xsl:when test="count(/instructionsection/classes/iclass/encoding[./equivalent_to/aliascond/text() = 'Never']) &gt; 0 and
                          count(/instructionsection/classes/iclass/encoding[./equivalent_to/aliascond/text() != 'Never']) &gt; 0">
            <xsl:text>a pseudo-instruction or an alias</xsl:text>
          </xsl:when>
          <xsl:when test="/instructionsection/classes/iclass/encoding[./equivalent_to/aliascond/text() = 'Never']">
            <xsl:text>a pseudo-instruction</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>an alias</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <p>
        This is <xsl:value-of select="$alias_and_or_pseudo" /> of
        <xsl:apply-templates select="/instructionsection/aliasto"/>.
        This means:
      </p>
      <ul>
        <li>
          The encodings in this description are named to match the encodings of
          <xsl:apply-templates select="/instructionsection/aliasto"/>.
        </li>
        <xsl:if test="$alias_and_or_pseudo = 'a pseudo-instruction'">
          <li>
            The assembler syntax is used only for assembly, and is not used on disassembly.
          </li>
        </xsl:if>
        <li>
          <xsl:text>The description of </xsl:text>
          <xsl:apply-templates select="/instructionsection/aliasto"/>
          <xsl:text> gives the operational pseudocode, any </xsl:text>
          <span class="arm-defined-word">constrained unpredictable</span>
          <xsl:text> behavior, and any operational information for this instruction.</xsl:text>
        </li>
      </ul>
    </xsl:if>

    <!-- the encodings for this instruction -->
    <xsl:apply-templates select="/instructionsection/classes"/>

    <!-- the assembler symbols -->
    <xsl:apply-templates select="/instructionsection/explanations"/>

    <!-- the table of aliases that are based on this instruction -->
    <xsl:call-template name="alias-table"/>

    <!-- the operational pseudocode -->
    <xsl:if test="/instructionsection/aliasto and not(/instructionsection/ps_section)">
      <!-- on an alias page, the pseudocode is on the machine instruction -->
      <div class="alias_ps_section">
        <h3 class="pseudocode">Operation</h3>
        <p><xsl:text>The description of </xsl:text>
          <xsl:apply-templates select="/instructionsection/aliasto"/>
          <xsl:text> gives the operational pseudocode for this instruction.</xsl:text></p>
      </div>
    </xsl:if>
    <!-- otherwise it's not an alias page, or we have the pseudocode -->
    <xsl:apply-templates select="/instructionsection/ps_section"/>
    <xsl:if test="/instructionsection/constrained_unpredictables">
      <xsl:apply-templates select="constrained_unpredictables"/>
    </xsl:if>

    <!-- the exceptions this instruction can cause -->
    <xsl:apply-templates select="/instructionsection/exceptions"/>

    <!-- any notes from AML on the operation of the instruction, e.g. fixed timing -->
    <xsl:apply-templates select="/instructionsection/operationalnotes"/>

    <!-- expand other information in the XML on the operation of the instruction. Currectly these might be:
           - uses_dit (fixed timing, in Armv9 only)
           - takes_movprfx (instruction preceded by MOVPRFX)
           - affected_by_sme (effects of SME on subsequent instructions when in Streaming SVE mode)  -->
    <xsl:if test="/instructionsection/desc/uses_dit='True' or /instructionsection/desc/takes_movprfx='True' or /instructionsection/desc/affected_by_sme">
      <xsl:if test="not(/instructionsection/operationalnotes)">
        <h3>Operational information</h3>
      </xsl:if>
      <xsl:if test="/instructionsection/desc/uses_dit='True'">
        <xsl:variable name="uses_dit_statement">
          <xsl:choose>
            <xsl:when test="/instructionsection/desc/predicated='True'">
              <xsl:choose>
                <xsl:when test="/instructionsection/desc/is_gov_pred_pair='True'">
                  <li>The values of the data supplied in any of its operand registers when its governing predicate registers contain the same value for each execution.</li>
                </xsl:when>
                <xsl:otherwise>
                  <li>The values of the data supplied in any of its operand registers when its governing predicate register contains the same value for each execution.</li>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
            <xsl:otherwise>
              <li>The values of the data supplied in any of its registers.</li>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:element name="p">
          <xsl:attribute name="class">aml</xsl:attribute>
          <xsl:choose>
            <xsl:when test="/instructionsection/desc/uses_dit/@condition and not(contains(/instructionsection/classes/iclass/arch_variants/arch_variant/@feature, 'FEAT_SVE')) and not(contains(/instructionsection/classes/iclass/arch_variants/arch_variant/@feature, 'FEAT_SME'))">
                <xsl:text>If </xsl:text><xsl:value-of select="/instructionsection/desc/uses_dit/@condition"/><xsl:text>, then i</xsl:text>
            </xsl:when>
            <xsl:otherwise>
                <xsl:text>I</xsl:text>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:text>f PSTATE.DIT is 1:</xsl:text>
        </xsl:element> <!-- p -->
        <ul>
          <li>The execution time of this instruction is independent of:
            <ul>
              <xsl:copy-of select="$uses_dit_statement"/>
              <li>The values of the NZCV flags.</li>
            </ul>
          </li>
          <li>The response of this instruction to asynchronous exceptions does not vary based on:
            <ul>
              <xsl:copy-of select="$uses_dit_statement"/>
              <li>The values of the NZCV flags.</li>
            </ul>
          </li>
        </ul>
      </xsl:if>
      <xsl:if test="/instructionsection/desc/takes_movprfx='True'">
        <p class="aml">
          This instruction might be immediately preceded in program order by a <span class="asm-code">MOVPRFX</span> instruction. The <span class="asm-code">MOVPRFX</span> instruction must conform to all of the following requirements, otherwise the behavior of the <span class="asm-code">MOVPRFX</span> and this instruction is <span class="arm-defined-word">unpredictable</span>:
        </p>
        <ul>
          <xsl:choose>
            <xsl:when test="/instructionsection/desc/takes_pred_movprfx='True'">
              <xsl:choose>
                <xsl:when test="/instructionsection/desc/is_wide_zm='True'">
                  <li>The <span class="asm-code">MOVPRFX</span> instruction must be unpredicated, or be predicated using the same governing predicate register and destination element size as this instruction.</li>
                </xsl:when>
                <xsl:otherwise>
                  <li>The <span class="asm-code">MOVPRFX</span> instruction must be unpredicated, or be predicated using the same governing predicate register and source element size as this instruction.</li>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
            <xsl:otherwise>
              <li>The <span class="asm-code">MOVPRFX</span> instruction must be unpredicated.</li>
            </xsl:otherwise>
          </xsl:choose>
          <li>The <span class="asm-code">MOVPRFX</span> instruction must specify the same destination register as this instruction.</li>
          <li>The destination register must not refer to architectural register state referenced by any other source operand register of this instruction.</li>
        </ul>
      </xsl:if>
      <xsl:if test="/instructionsection/desc/affected_by_sme">
        <p class="aml">
          <xsl:text>If FEAT_SME is implemented and the PE is in Streaming SVE mode, then any subsequent instruction which is dependent on the </xsl:text>
          <xsl:value-of select="/instructionsection/desc/affected_by_sme/@output"/>
          <xsl:text> written by this instruction might be significantly delayed.</xsl:text>
        </p>
      </xsl:if>
    </xsl:if>

    <hr/>
    <xsl:call-template name="generate_footer"/>
    </body>

    </html>
  </xsl:template>

  <xsl:template match="/textsection">
    <html>

    <head>
      <link rel="stylesheet" type="text/css" href="insn.css"/>
      <meta name="generator" content="iform.xsl"/>
      <title><xsl:value-of select="@title"/></title>
    </head>

    <body>
    <xsl:call-template name="generate_header_footer"/>
    <hr/>

    <h1><xsl:value-of select="@title"/></h1>

    <xsl:apply-templates/>

    <hr/>
    <xsl:call-template name="generate_footer"/>

    </body>

    </html>
  </xsl:template>

  <xsl:template name="generate_header_footer">
    <table style="margin: 0 auto;">
      <tr>
        <!-- autogenerator: header/footer start -->
        <!-- autogenerated -->
	<td><div class="topbar"><a href="index.xml">Base Instructions</a></div></td>
	<td><div class="topbar"><a href="fpsimdindex.xml">SIMD&amp;FP Instructions</a></div></td>
	<td><div class="topbar"><a href="sveindex.xml">SVE Instructions</a></div></td>
	<td><div class="topbar"><a href="mortlachindex.xml">SME Instructions</a></div></td>
	<td><div class="topbar"><a href="encodingindex.xml">Index by Encoding</a></div></td>
	<td><div class="topbar"><a href="shared_pseudocode.xml">Shared Pseudocode</a></div></td>
	<td><div class="topbar"><a href="notice.xml">Proprietary Notice</a></div></td>
        <!-- autogenerator: header/footer end -->
      </tr>
    </table>
  </xsl:template>

  <xsl:template name="generate_footer">
    <xsl:call-template name="generate_header_footer"/>
    <!-- version footer -->
    <p class="versions">
      Internal version only: isa v33.59, AdvSIMD v29.12, pseudocode v2022-12_rel, sve v2022-12_relb
      ; Build timestamp: 2022-12-14T22:29
    </p>
    <p class="copyconf">
      Copyright &#169; 2010-2022 Arm Limited or its affiliates. All rights reserved.
      This document is Non-Confidential.
    </p>
  </xsl:template>

<!--
########################################################################
The remaining support code for the specific parts of the XML file
referenced above starts here. It contains the templates that are
necessary to transform individual parts of the XML into a useful
browseable view.
########################################################################
-->

  <xsl:template match="/instructionsection/heading">
    <h2 class="instruction-section"><xsl:value-of select="."/></h2>
  </xsl:template>

  <xsl:template match="desc">
    <xsl:choose>
      <xsl:when test="./authored">
        <xsl:apply-templates select="./authored"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="./brief"/>
        <xsl:apply-templates select="./alg"/>
        <xsl:apply-templates select="./intrinsics"/>
        <xsl:apply-templates select="./description"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="desc/authored">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="desc/brief">
    <p>
      <xsl:value-of select="."/>
      <xsl:apply-templates select="../longer"/>
      <xsl:if test="@orphanslink">
        .  See <a href="{@orphanslink}">orphan pseudocode</a> for pseudocode
        which belongs to encodings but is not yet linked to instruction pages
      </xsl:if>
    </p>
  </xsl:template>

  <xsl:template match="desc/alg">
    <p>
    <xsl:choose>
      <xsl:when test="@howmany > 1"><br /></xsl:when>
      <xsl:otherwise>: </xsl:otherwise>
    </xsl:choose>
    <span class="desc-alg"><xsl:value-of select="."/></span>.
    </p>
  </xsl:template>

  <xsl:template match="desc/longer">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template name="instruction_aliases_intro">
    <!-- Render introduction for optional list of aliases or pseudo-instructions. -->
    <xsl:param name="aliasrefs"/>
    <xsl:if test="count($aliasrefs) &gt; 0">
      <p class="desc">
        <xsl:text>This instruction is used by the </xsl:text>
        <xsl:choose>
          <xsl:when test="count($aliasrefs) &gt; 1">
            <!-- Multiple aliases or pseudo-instructions. -->
            <xsl:choose>
              <xsl:when test="$aliasrefs[1]/aliaspref/text() = 'Never'">
                <xsl:text>pseudo-instructions </xsl:text>
              </xsl:when>
              <xsl:otherwise>
                <xsl:text>aliases </xsl:text>
              </xsl:otherwise>
            </xsl:choose>
            <!-- Render each of them, doing the punctuation here.
                 Note: This ignores the @punct information generated in the XML. -->
            <xsl:for-each select="$aliasrefs">
              <xsl:if test="position() = last()">
                <xsl:text>and </xsl:text>
              </xsl:if>
              <xsl:apply-templates select="."/>
              <xsl:if test="position() != last()">
                <xsl:text>, </xsl:text>
              </xsl:if>
            </xsl:for-each>
          </xsl:when>
          <xsl:otherwise>
            <!-- A single alias or pseudo-instruction exists. -->
            <xsl:choose>
              <xsl:when test="$aliasrefs[1]/aliaspref/text() = 'Never'">
                <xsl:text>pseudo-instruction </xsl:text>
              </xsl:when>
              <xsl:otherwise>
                <xsl:text>alias </xsl:text>
              </xsl:otherwise>
            </xsl:choose>
            <xsl:apply-templates select="$aliasrefs"/>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:text>.</xsl:text>
      </p>
    </xsl:if>
  </xsl:template>

  <xsl:template match="instructionsection/alias_list">
    <!-- First render list of aliases. -->
    <xsl:call-template name="instruction_aliases_intro">
      <xsl:with-param name="aliasrefs" select="./aliasref[./aliaspref/text() != 'Never']"/>
    </xsl:call-template>
    <!-- Next render list of pseudo-instructions.
         The difference between an alias and a pseudo-instruction is
         in whether the assembler syntax is seen on disassembly. -->
    <xsl:call-template name="instruction_aliases_intro">
      <xsl:with-param name="aliasrefs" select="./aliasref[./aliaspref/text() = 'Never']"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="alias_list_intro">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="alias_list_outro">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="instructionsection/alias_list/aliasref">
    <a href="{@aliasfile}" title="{@hover}">
      <xsl:apply-templates select="./text" />
    </a>
  </xsl:template>

  <xsl:template match="aliasref/aliaspref">
    <!-- nothing output for aliaspref in sequence, used later for a table -->
  </xsl:template>

  <xsl:template match="instructionsection/aliasto">
    <a href="{@refiform}"><xsl:value-of select="."/></a>
  </xsl:template>

  <xsl:template match="/instructionsection/classes">
    <xsl:apply-templates />

<!--
    The following commented-out XSL is a demonstration of how you might replace the
    Perl in iform2xml.pl that generates the 'classesintro' XML.  Instead, the XSL uses
    the information present in the source XML to generate the appropriate introductory
    HTML directly.  This would have the advantage that you don't need to embed this
    (redundant) information in every XML document.

    The current implementation simply replicates the original (classic, A64) behaviour
    of the Perl code.  That is pretty easy, because it only involves looping over the
    'iclass' nodes and outputting simplifying transforms to the nodes.  If you want to
    replicate the 'new' A32/T32 Perl from iform2xml.pl, you will need to find a way to
    simulate a dynamic associative array (hash).  This doesn't come naturally to XSLT,
    because it does not let you change the value of a variable once set, and it does not
    support array variables.  To overcome the first problem, I think the way to go would
    be to use recursion, and add to the passed hash parameter each time you go into the
    recursion.  Emulating a hash could probably be done by building up suitable XML.
    This is what would be passed as a parameter to the recursion.  When the recursion
    detects the termination condition (no more iclass nodes to iterate over) it would
    then use a bunch of 'xsl:value-of' tags to output the contents of the hash in the
    desired format.
 -->
<!--
    <xsl:apply-templates select="classesintro" />
    <xsl:variable name="iclass_count" select="count(iclass)"/>
    <p class="desc">It has encodings from <xsl:value-of select="$iclass_count" /> classes:
    <xsl:for-each select="iclass">
      <xsl:variable name="iclass_id" select="@id" />
      <xsl:variable name="iclass_name" select="@name" />
      <a>
        <xsl:attribute name="href">#<xsl:value-of select="$iclass_id" /></xsl:attribute>
        <xsl:value-of select="$iclass_name"/>
      </a>
      <xsl:choose><xsl:when test="position() != last()">, </xsl:when></xsl:choose>
    </xsl:for-each>
    </p>
    <xsl:apply-templates select="iclass" />
 -->
    <div class="encoding-notes">
      <xsl:apply-templates select="/instructionsection/desc/encodingnotes"/>
    </div>
  </xsl:template>

  <xsl:template match="/instructionsection/classes/classesintro">
    <xsl:if test="@count > 1">
      <p class="desc"><xsl:apply-templates /></p>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/instructionsection/ps_section">
    <xsl:if test="@howmany > 1">
      <h2 class="pseudocode">Pseudocodes</h2>
    </xsl:if>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="/instructionsection/ps_section/ps">
    <div class="ps">
      <a id="{@mylink}"></a>
      <h3 class="pseudocode">
        <xsl:choose>
          <xsl:when test="@heading">
            <xsl:value-of select="@heading"/>
          </xsl:when>
          <xsl:when test="@secttype and @secttype != 'Library'">
            <xsl:value-of select="@secttype"/>
          </xsl:when>
          <xsl:when test="@secttype and @secttype = 'Library'">
            Library pseudocode for <xsl:value-of select="@name"/>
          </xsl:when>
          <xsl:when test="@sections = 1"></xsl:when>
          <xsl:otherwise>
            Pseudocode
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="@diagram and ../@howmany > 1" >
          for encodings: <xsl:value-of select="@enclabels"/>
        </xsl:if>
      </h3>
      <xsl:if test = "@diagram">
        <p>Symbols used in the pseudocode can be found in this diagram:</p>
      </xsl:if>
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template match="pstext">
    <xsl:if test="not(../@secttype)">
      <h4 class="pseudocode">
        <xsl:value-of select="@section"/> pseudocode
      </h4>
    </xsl:if>
    <xsl:choose>
      <xsl:when test="@mayhavelinks">
        <p class="pseudocode">
          <xsl:apply-templates/>
        </p>
      </xsl:when>
      <xsl:otherwise>
        <p class="pseudocode">
          <xsl:value-of select="."/>
        </p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="txt">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="text">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="pstext/anchor">
    <a id="{@link}"></a>
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="a">
    <xsl:choose>
      <xsl:when test="@class = 'iclass'">
        <a href="encodingindex.xml#{../../@id}"><xsl:value-of select="."/></a>
      </xsl:when>
      <xsl:when test="@link">
        <a href="{@file}#{@link}" title="{@hover}"><xsl:value-of select="."/></a>
      </xsl:when>
      <xsl:otherwise>
        <xsl:choose>
          <xsl:when test="@href">
            <a href="{@href}"><xsl:value-of select="."/></a>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="."/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="constrained_unpredictables">
    <!-- This describes the behaviours in CONSTRAINED UNPREDICTABLE situations,
         and would normally come after a pseudocode section. -->
    <xsl:if test="count(.) > 0">
      <h3>CONSTRAINED UNPREDICTABLE behavior</h3>
      <xsl:for-each select="./cu_case">
        <p>If <span class="pseudocode"><xsl:value-of select="./cu_cause/pstext"/></span>, then one of the following behaviors must occur:</p>
        <ul>
        <xsl:for-each select="./cu_type">
          <xsl:call-template name="cu_type">
            <xsl:with-param name="cu_type" select="."/>
          </xsl:call-template>
        </xsl:for-each>
        </ul>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template name="cu_type">
    <!-- Describes a single behaviour that can occur in a CONSTRAINED UNPREDICTABLE
         situation. -->
    <xsl:param name="cu_type"/>
    <xsl:variable name="constraint_text">
      <xsl:if test="$cu_type/@constraint">
        <xsl:call-template name="pick_constraint_text">
          <xsl:with-param name="constraint" select="$cu_type/@constraint"/>
        </xsl:call-template>
      </xsl:if>
      <xsl:if test="$cu_type/cu_type_text">
        <xsl:apply-templates select="$cu_type/cu_type_text"/>
      </xsl:if>
    </xsl:variable>
    <xsl:variable name="constraint_text_with_variables">
      <xsl:call-template name="insert_variables">
        <xsl:with-param name="text" select="$constraint_text"/>
        <xsl:with-param name="variables" select="$cu_type/cu_type_variable"/>
      </xsl:call-template>
    </xsl:variable>
    <li><xsl:copy-of select="$constraint_text_with_variables"/></li>
  </xsl:template>

  <xsl:template name="pick_constraint_text">
    <!-- Find a suitable piece of humanly-readable text matching the supplied
         constraint ID. -->
    <xsl:param name="constraint"/>
    <xsl:variable name="constraint_text_mapping" select="document('constraint_text_mappings.xml')/constraint_text_mappings/constraint_text_mapping[./constraint_id = $constraint]"/>
    <xsl:choose>
      <xsl:when test="$constraint_text_mapping">
        <xsl:apply-templates select="$constraint_text_mapping/constraint_text"/>
      </xsl:when>
      <xsl:otherwise>
        Error: unknown constraint: <xsl:value-of select="$constraint"/>.
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="insert_variables">
    <!-- Insert variables from a list into a text string. E.g. if the source text
         looks like this:

         The instruction behaves as if $assembler.

         ...and one of the variables has the name 'assembler' (without a
         dollar sign at the beginning), and a value 'sz == 10', then the
         new text string returned by this template will be:

         The instruction behaves as if sz == 10.

         It will work with multiple variables, but their names should be unique.
         -->
    <xsl:param name="text"/>
    <xsl:param name="variables"/>
    <xsl:choose>
      <xsl:when test="$variables and count($variables) != 0">
        <xsl:variable name="name">
          <xsl:value-of select="concat('$',$variables[1]/@name)"/>
        </xsl:variable>
        <xsl:variable name="value">
          <xsl:value-of select="$variables[1]/@value"/>
        </xsl:variable>
        <xsl:variable name="new_text">
          <xsl:copy-of select="substring-before($text,$name)"/>
          <xsl:value-of select="$value"/>
          <xsl:copy-of select="substring-after($text,$name)"/>
        </xsl:variable>
        <xsl:call-template name="insert_variables">
          <xsl:with-param name="text" select="$new_text"/>
          <xsl:with-param name="variables" select="$variables[position() &gt; 1]"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:copy-of select="$text"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="/instructionsection/classes/iclass">
    <xsl:if test="(@oneof > 1)
                  or (@isa = 'A32') or (@isa = 'T32')
                  or count(./arch_variants/*) &gt; 0">
      <h3 class="classheading">
        <a id="{@id}"></a>
        <xsl:value-of select="@name"/>
        <xsl:if test="count(./arch_variants/*) &gt; 0">
          <span style="font-size:smaller;">
            <br/>(<xsl:apply-templates select="./arch_variants"/>)
          </span>
        </xsl:if>
      </h3>
    </xsl:if>
    <!-- Intro text -->
    <xsl:apply-templates select="./iclassintro"/>
    <!-- Diagram -->
    <xsl:apply-templates select="./regdiagram"/>
    <!-- Assembler syntax for the encoding -->
    <xsl:apply-templates select="./encoding"/>
    <!-- Decode pseudocode -->
    <xsl:apply-templates select="./ps_section/ps/pstext"/>
    <xsl:if test="./constrained_unpredictables">
      <xsl:apply-templates select="constrained_unpredictables"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="arch_variants">
    <xsl:for-each select="./arch_variant">
      <xsl:if test="position() != 1"><xsl:text>, </xsl:text></xsl:if>
      <xsl:choose>
        <xsl:when test="@feature">
          <xsl:value-of select="./@feature"/>
        </xsl:when>
        <xsl:when test="@name">
          <xsl:choose>
            <xsl:when test="substring(./@name,1,3)='ARM'">
              <xsl:text>Arm</xsl:text><xsl:value-of select="substring(./@name,4)"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="./@name"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          ERROR
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match ="iclass/iclassintro">
    <xsl:choose>
      <xsl:when test="@count > 1">
        <p class="desc"><xsl:apply-templates /></p>
      </xsl:when>
      <xsl:when test="../@oneof = 1">
        <p class="desc"><xsl:apply-templates /></p>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match ="iclass/encoding">
    <div class="encoding">
      <!-- Removed as result of AARCH-4381, where the 'label' was
           invisible in the browseable XML, but is not invisible in
           the Armv8 ARM. So, any incorrect labels are hard to spot.

           It is also necessary for A32/T32 XML where the encoding
           names may be referenced.
      -->
      <!--
      <xsl:if test="(@oneofinclass > 1) or (@bitdiffs)">
      -->
        <h4 class="encoding">
          <xsl:value-of select="@label"/>
          <xsl:if test="@bitdiffs">
            <span class="bitdiff"> (<xsl:value-of select="@bitdiffs"/>)</span>
          </xsl:if>
          <xsl:if test="count(./arch_variants/*) &gt; 0">
            <span style="font-size:smaller;">
              <br/>(<xsl:apply-templates select="./arch_variants"/>)
            </span>
          </xsl:if>
        </h4>
      <!--
      </xsl:if>
      -->

      <!-- Render the assembler template for this encoding and any
           additional information for aliases and pseudo-instructions.
      -->
      <xsl:if test="@name">
        <a id="{@name}"></a>
      </xsl:if>
      <xsl:apply-templates select="asmtemplate"/>
      <xsl:apply-templates select="equivalent_to"/>
    </div>
  </xsl:template>

  <xsl:template match ="iclass/encoding/box">
    <!-- no output here, this box is for program use -->
  </xsl:template>

  <xsl:template match="encoding/equivalent_to">
    <p class="equivto">
      is equivalent to
    </p>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="encoding/equivalent_to/aliascond">
    <xsl:choose>
      <xsl:when test=".='Unconditionally'">
        <p class="equivto">
          and is always the preferred disassembly.
        </p>
      </xsl:when>
      <xsl:when test=".='Never'">
        <xsl:if test="count(/instructionsection/classes/iclass/encoding[./equivalent_to/aliascond/text() = 'Never']) != count(/instructionsection/classes/iclass/encoding)">
          <p class="equivto">
            but is never the preferred disassembly.
          </p>
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <p class="equivto">
          and is the preferred disassembly when
          <span class="pseudocode"><xsl:value-of select="."/></span>.
        </p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="asmtemplate">
    <p class="asm-code">
      <xsl:apply-templates />
      <xsl:if test="../@pslink or @comment">
        //
      </xsl:if>
      <xsl:if test="../@pslink">
        <a href="#{../@pslink}">pseudocode</a>
      </xsl:if>
      <xsl:if test="@comment">
        (<xsl:value-of select="@comment"/>)
      </xsl:if>
    </p>
  </xsl:template>

  <xsl:template match="encoding/oldalias">
    <p class="alias-explanation">
      This encoding
      <xsl:choose>
        <xsl:when test="@preference = 'preferred'">
          should
        </xsl:when>
        <xsl:when test="@preference = 'deprecated'">
          may (with deprecation)
        </xsl:when>
        <xsl:otherwise>
          might (subject to <xsl:value-of select="@preference"/>)
        </xsl:otherwise>
      </xsl:choose>
      be written using the alias mnemonic <xsl:value-of select="@mnemonic"/>
      <xsl:if test="@conditions != ''">
        when <xsl:value-of select="@conditions"/>
      </xsl:if>
      .  Write it as:
    </p>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="explanations">
    <xsl:if test="explanation">
      <xsl:if test="@scope='all'">
        <h3 class="explanations">Assembler Symbols</h3>
      </xsl:if>

      <div class="explanations">
        <xsl:for-each select="explanation[@symboldefcount = 1]">
          <xsl:variable name="symbolname" select="symbol"/>
          <table>
            <col class="asyn-l"/><col class="asyn-r"/>
            <tr>
              <xsl:apply-templates select="."/>
            </tr>
            <xsl:for-each select="../explanation[@symboldefcount > 1]">
              <xsl:if test="symbol = $symbolname">
                <tr>
                  <xsl:apply-templates select="."/>
                </tr>
              </xsl:if>
            </xsl:for-each>
          </table>
        </xsl:for-each>
      </div>

      <div class="syntax-notes">
        <xsl:apply-templates select="/instructionsection/desc/syntaxnotes"/>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template match="aliastablelink">
    <a>
      <xsl:attribute name="href">#<xsl:value-of select="//aliastablehook/@anchor"/></xsl:attribute>
      <xsl:value-of select="//aliastablehook/."/>
    </a>
  </xsl:template>

  <xsl:template name="alias-table">
    <xsl:if test='//alias_list/@howmany > 0 and //alias_list/aliasref/aliaspref/text() != "Unconditionally" and //alias_list/aliasref/aliaspref/text() != "Never"'>
      <h3 class="aliastable" id="{@anchor}">Alias Conditions</h3>

      <!-- insert alias table here.  The table data comes from the
           'alias_list' element which was at the start of the file, so
           we'll use it out of sequence. -->

      <table class="aliastable">
        <thead>
          <tr>
            <!-- deprecated to put text in this file, sorry -->
            <th>Alias</th>
            <xsl:if test="//alias_list/aliasref/aliaspref[@labels]">
              <th>Of variant</th>
            </xsl:if>
            <th>Is preferred when</th>
          </tr>
        </thead>
        <tbody>
          <xsl:for-each select="//alias_list/aliasref/aliaspref">
            <tr>
              <td>
                <a href="{../@aliasfile}" title="{../@hover}">
                  <xsl:value-of select="../text"/>
                </a>
              </td>
              <xsl:if test="//alias_list/aliasref/aliaspref[@labels]">
                <td class="notfirst"><xsl:value-of select="@labels"/></td>
              </xsl:if>
              <td class="notfirst"><span class="pseudocode"><xsl:apply-templates /></span></td>
            </tr>
          </xsl:for-each>
        </tbody>
      </table>
    </xsl:if>
  </xsl:template>

  <xsl:template match="explanation_intro">
    <xsl:choose>
      <xsl:when test="@headingsabove > 1">
        <h4 class="encoding"><xsl:value-of select="."/></h4>
      </xsl:when>
      <xsl:otherwise>
        <p class="where"><xsl:value-of select="."/></p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="encoding/diagramtemplate">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="regdiagram">
    <div class="regdiagram-{@form}">
    <table class="regdiagram">
      <thead>
        <tr>
          <xsl:choose>
            <xsl:when test="@form = '16x2'">
              <td>15</td><td>14</td><td>13</td><td>12</td><td>11</td><td>10</td><td>9</td><td>8</td><td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td><td>15</td><td>14</td><td>13</td><td>12</td><td>11</td><td>10</td><td>9</td><td>8</td><td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
            </xsl:when>
            <xsl:when test="@form = '16'">
              <td>15</td><td>14</td><td>13</td><td>12</td><td>11</td><td>10</td><td>9</td><td>8</td><td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
            </xsl:when>
            <xsl:otherwise>
              <td>31</td><td>30</td><td>29</td><td>28</td><td>27</td><td>26</td><td>25</td><td>24</td><td>23</td><td>22</td><td>21</td><td>20</td><td>19</td><td>18</td><td>17</td><td>16</td><td>15</td><td>14</td><td>13</td><td>12</td><td>11</td><td>10</td><td>9</td><td>8</td><td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
            </xsl:otherwise>
          </xsl:choose>
        </tr>
      </thead>
      <tbody>
        <tr class="firstrow">
          <xsl:for-each select="./box">
            <xsl:choose>
              <xsl:when test="@usename and not (@settings)">
                <td>
                  <xsl:if test="@width > 1">
                    <xsl:attribute name="colspan"><xsl:value-of select="@width"/></xsl:attribute>
                  </xsl:if>
                  <xsl:attribute name="class">lr</xsl:attribute>
                  <xsl:value-of select="@name"/>
                </td>
              </xsl:when>
              <xsl:otherwise>
                <xsl:for-each select="./c">
                  <td>
                    <xsl:if test="@colspan">
                      <xsl:attribute name="colspan"><xsl:value-of select="@colspan"/></xsl:attribute>
                    </xsl:if>
                    <xsl:choose>
                      <xsl:when test="position() = 1 and last() = 1">
                        <xsl:attribute name="class">lr</xsl:attribute>
                      </xsl:when>
                      <xsl:when test="position() = 1">
                        <xsl:attribute name="class">l</xsl:attribute>
                      </xsl:when>
                      <xsl:when test="position() = last()">
                        <xsl:attribute name="class">r</xsl:attribute>
                      </xsl:when>
                    </xsl:choose>
                    <xsl:value-of select="."/>
                  </td>
                </xsl:for-each>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:for-each>
        </tr>
        <xsl:if test="@tworows">
          <tr class="secondrow">
            <xsl:for-each select="./box">
              <td>
                <xsl:if test="@width > 1">
                  <xsl:attribute name="colspan">
                    <xsl:value-of select="@width"/>
                  </xsl:attribute>
                </xsl:if>
                <xsl:if test="@settings and @usename">
                  <xsl:attribute name="class">droppedname</xsl:attribute>
                  <xsl:value-of select="@name"/>
                </xsl:if>
              </td>
            </xsl:for-each>
          </tr>
        </xsl:if>
      </tbody>
    </table>
    </div>
  </xsl:template>

  <xsl:template match="explanation">
    <xsl:apply-templates select="symbol"/>
    <xsl:apply-templates select="account"/>
    <xsl:apply-templates select="definition"/>
  </xsl:template>

  <xsl:template match="explanation/symbol">
    <td>
      <xsl:if test="not(../@symboldefcount) or ../@symboldefcount = 1">
        <xsl:value-of select="."/>
      </xsl:if>
    </td>
  </xsl:template>

  <xsl:template match="explanation/account">
    <td>
      <a id="{../symbol/@link}"></a>
      <xsl:apply-templates/>
    </td>
  </xsl:template>

  <xsl:template match="explanation/account/intro">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="explanation/account/bullets">
    <ul class="accountbullets">
      <xsl:apply-templates/>
    </ul>
  </xsl:template>

  <xsl:template match="explanation/account/bullets/bullet">
    <li><xsl:value-of select="."/></li>
  </xsl:template>

  <xsl:template match="explanation/definition">
    <td>
      <a id="{../symbol/@link}"></a>
      <xsl:apply-templates/>
    </td>
  </xsl:template>

  <xsl:template match="definition/intro">
    <p>
      <xsl:apply-templates/>
      encoded in
      <xsl:choose>
        <xsl:when test="../@tabulatedwith">
          <q><xsl:value-of select="../@encodedin"/></q>, based on
          <q><xsl:value-of select="../@tabulatedwith"/></q>:
        </xsl:when>
        <xsl:otherwise>
          <q><xsl:value-of select="../@encodedin"/></q>:
        </xsl:otherwise>
      </xsl:choose>
    </p>
  </xsl:template>

  <xsl:template match="definition/after">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="table">
    <table class="{@class}">
      <xsl:apply-templates/>
    </table>
  </xsl:template>

  <xsl:template match="thead">
    <thead>
      <xsl:apply-templates/>
    </thead>
  </xsl:template>

  <xsl:template match="row">
    <tr>
      <xsl:apply-templates/>
    </tr>
  </xsl:template>

  <xsl:template match="thead/row/entry">
    <th class="{@class}">
      <xsl:apply-templates/>
    </th>
  </xsl:template>

  <xsl:template match="tbody">
    <tbody>
      <xsl:apply-templates/>
    </tbody>
  </xsl:template>

  <xsl:template match="tbody/row/entry">
    <td class="{@class}">
      <xsl:choose>
        <xsl:when test="@iclasslink">
          <a href="{@iclasslinkfile}#{@iclasslink}">
            <xsl:apply-templates/>
          </a>
        </xsl:when>
        <xsl:when test="@class='feature' and count(arch_variants)=0">
          -
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates/>
        </xsl:otherwise>
      </xsl:choose>
    </td>
  </xsl:template>

  <xsl:template match="aliasmnem">
    <h3 class="alias-section">
      <a id="{@id}"></a>
      <xsl:value-of select="@heading"/> alias
    </h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="aliasmnem/aliases">
    <table class="aliases">
      <thead>
        <tr>
          <th>Alias</th>
          <th>
            Is preferred to<xsl:if test="@conditions"> (if
            <xsl:value-of select="@conditions"/>)</xsl:if>:
          </th>
        </tr>
      </thead>
      <tbody>
        <xsl:apply-templates/>
      </tbody>
    </table>
  </xsl:template>

  <xsl:template match="aliasmnem/aliases/alias">
    <tr class="alias" alias="">
      <xsl:choose>
        <xsl:when test="asmtemplate">
          <xsl:for-each select="asmtemplate">
            <td class="asm-code">
              <xsl:if test="@role='alias_equivalent_to'">
                <xsl:attribute name="notfirst"></xsl:attribute>
              </xsl:if>
              <xsl:apply-templates/>
            </td>
          </xsl:for-each>
        </xsl:when>
        <xsl:otherwise>
          <td class="asm-code">
            <xsl:value-of select="@assembler_prototype"/>
          </td>
          <td class="asm-code" notfirst="1">
            <xsl:value-of select="@equivalent_to"/>
          </td>
        </xsl:otherwise>
      </xsl:choose>
    </tr>
  </xsl:template>

  <xsl:template match="aliasmnem/alias">
    <p class="alias-explanation">
      <xsl:if test="@enctag">
        <em><xsl:value-of select="@enctag"/> encodings</em>:
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@conditions">
          When <xsl:value-of select="@conditions"/>, the
        </xsl:when>
        <xsl:otherwise>
          The
        </xsl:otherwise>
      </xsl:choose>
      instruction written:
    </p>
    <p class="asm-code"><xsl:value-of select="@assembler_prototype"/></p>
    <p class="alias-explanation">is equivalent to:</p>
    <p class="asm-code"><xsl:value-of select="@equivalent_to"/>.</p>
  </xsl:template>

  <xsl:template match="operation">
    <h3>Operation</h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="operationalnotes">
    <h3>Operational information</h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="exceptions">
    <h3>Exceptions</h3>
    <xsl:choose>
      <xsl:when test="./exception_group">
        <!-- If we have any exception groups, apply templates to print them. -->
        <xsl:apply-templates select="exception_group"/>
      </xsl:when>
      <xsl:otherwise>
        <p>None.</p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="exception_group">
    <p>
      <xsl:if test="./@group_name">
        <xsl:value-of select="./@group_name"/> exceptions:
      </xsl:if>
      <xsl:for-each select="./exception">
        <!-- The following sort is a bit long - unfortunately you can't use variables in xsl:sort select. -->
        <xsl:sort select="document('exception-conversion.xml')//exception[@register = current()/@register][not(@field) or @field = current()/@field][not(@value) or @value = current()/@value]/@text"/>
        <xsl:variable name="REGISTER" select="./@register"/>
        <xsl:variable name="FIELD" select="./@field"/>
        <xsl:variable name="VALUE" select="./@value"/>
        <xsl:variable name="NAME" select="./@name"/>
        <xsl:if test="position() != 1">
          <xsl:text>, </xsl:text> <!-- Comma before every entry other than the first. -->
        </xsl:if>
        <xsl:choose>
          <xsl:when test="./@register">
            <xsl:value-of select="document('exception-conversion.xml')//exception[@register = $REGISTER][not(@field) or @field = $FIELD][not(@value) or @value = $VALUE]/@text"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="document('exception-conversion.xml')//exception[@pseudocode = $NAME]/@text"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
      <xsl:text>.</xsl:text>
    </p>
  </xsl:template>

  <!-- Block and inline elements generated from AML processing -->

  <xsl:template match="fromaml">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="p">
    <p class="aml">
      <xsl:apply-templates/>
    </p>
  </xsl:template>

  <xsl:template match="para">
    <p class="aml">
      <xsl:apply-templates/>
    </p>
  </xsl:template>

  <xsl:template match="onepara">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="list">
    <xsl:choose>
      <xsl:when test="./@type = 'unordered'">
        <ul>
          <xsl:apply-templates/>
        </ul>
      </xsl:when>
      <xsl:when test="./@type = 'param'">
        <dl>
          <xsl:apply-templates/>
        </dl>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="listitem">
    <xsl:choose>
      <xsl:when test="../@type = 'unordered'">
        <li><xsl:apply-templates/></li>
      </xsl:when>
      <xsl:when test="../@type = 'param'">
        <dt><xsl:apply-templates select="param"/></dt>
        <dd><xsl:apply-templates select="content"/></dd>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="param">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="content">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="b">
    <b><xsl:value-of select="."/></b>
  </xsl:template>

  <xsl:template match="instruction">
    <span class="asm-code"><xsl:value-of select="."/></span>
  </xsl:template>

  <xsl:template match="xref">
    <a class="armarm-xref" title="Reference to Armv8 ARM section"><xsl:value-of select="."/></a>
  </xsl:template>

  <xsl:template match="arm-defined-word">
    <span class="arm-defined-word"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="sup">
    <sup><xsl:apply-templates/></sup>
  </xsl:template>

  <xsl:template match="sub">
    <sub><xsl:apply-templates/></sub>
  </xsl:template>
  
  <xsl:template match="note">
    <div class="note">
      <hr class="note"/>
      <h4>Note</h4>
      <xsl:apply-templates/>
      <hr class="note"/>
    </div>
  </xsl:template>

  <xsl:template match="image">
    The following figure shows an example of the operation of <xsl:value-of select="@label"/>.<br/>
    <img src="{@file}" alt="{@label}"/>
  </xsl:template>

  <xsl:template match="url">
    <a>
      <xsl:attribute name="href">
        <xsl:value-of select="text()"/>
      </xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

</xsl:stylesheet>
